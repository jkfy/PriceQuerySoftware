-- =============================================================================
--  电气公司报价查询系统 · 业务规则脚本（02）
--  内容：BOM 结构合法性校验（触发器 + 存储过程）
--  依赖：01_schema.sql 已执行
-- =============================================================================

USE `price_query`;
SET NAMES utf8mb4;

-- -----------------------------------------------------------------------------
-- 1. 触发器：拦截明显非法的 BOM 关系
--    规则：
--      a) 不能包含自身
--      b) 元件是不可再分的最小单位，不能有子件
--      c) 产品是最终交付物，不能出现在别人的子件位置上
--    环检测见 2 节的存储过程（触发器内做递归查询语义不清，且不利于报错信息）。
-- -----------------------------------------------------------------------------
DROP TRIGGER IF EXISTS `trg_bom_edge_bi`;
DROP TRIGGER IF EXISTS `trg_bom_edge_bu`;

DELIMITER $$

CREATE TRIGGER `trg_bom_edge_bi`
BEFORE INSERT ON `bom_edge`
FOR EACH ROW
BEGIN
  DECLARE v_parent_type VARCHAR(16) DEFAULT NULL;
  DECLARE v_child_type  VARCHAR(16) DEFAULT NULL;

  IF NEW.`parent_id` = NEW.`child_id` THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'BOM 校验失败：物料不能包含自身';
  END IF;

  SELECT `item_type` INTO v_parent_type FROM `item` WHERE `id` = NEW.`parent_id`;
  SELECT `item_type` INTO v_child_type  FROM `item` WHERE `id` = NEW.`child_id`;

  IF v_parent_type = 'component' THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'BOM 校验失败：元件不可再分，不能包含子件';
  END IF;

  IF v_child_type = 'product' THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'BOM 校验失败：产品不能作为其它物料的子件';
  END IF;
END$$

CREATE TRIGGER `trg_bom_edge_bu`
BEFORE UPDATE ON `bom_edge`
FOR EACH ROW
BEGIN
  DECLARE v_parent_type VARCHAR(16) DEFAULT NULL;
  DECLARE v_child_type  VARCHAR(16) DEFAULT NULL;

  IF NEW.`parent_id` = NEW.`child_id` THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'BOM 校验失败：物料不能包含自身';
  END IF;

  SELECT `item_type` INTO v_parent_type FROM `item` WHERE `id` = NEW.`parent_id`;
  SELECT `item_type` INTO v_child_type  FROM `item` WHERE `id` = NEW.`child_id`;

  IF v_parent_type = 'component' THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'BOM 校验失败：元件不可再分，不能包含子件';
  END IF;

  IF v_child_type = 'product' THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'BOM 校验失败：产品不能作为其它物料的子件';
  END IF;
END$$

-- -----------------------------------------------------------------------------
-- 2. 存储过程：新增/更新一条 BOM 关系（含环检测）
--    推荐所有 BOM 写操作都走这个入口，Qt 侧调用 CALL sp_bom_add_edge(...)。
--    注意：过程内部不控制事务，由调用方（Qt）负责 BEGIN / COMMIT / ROLLBACK，
--          这样一次「改 BOM + 重算 + 写审计」才能原子完成。
--    环检测用递归 CTE（需要 MySQL 8.0）。若目标环境版本较低、不支持存储过程内的
--    WITH，可删掉该段，改在 C++ 端用同样的递归逻辑先校验再插入，效果一致。
-- -----------------------------------------------------------------------------
DROP PROCEDURE IF EXISTS `sp_bom_add_edge`$$

CREATE PROCEDURE `sp_bom_add_edge`(
  IN p_parent_id BIGINT UNSIGNED,
  IN p_child_id  BIGINT UNSIGNED,
  IN p_quantity  DECIMAL(18,4),
  IN p_seq_no    INT,
  IN p_remark    VARCHAR(255)
)
COMMENT '新增/更新 BOM 关系，含循环包含检测。调用方负责事务。'
BEGIN
  DECLARE v_hit INT DEFAULT 0;

  IF p_quantity IS NULL OR p_quantity <= 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'BOM 校验失败：用量必须大于 0';
  END IF;

  -- 直接环：A 已包含 B，现在又要 B 包含 A
  SELECT COUNT(*) INTO v_hit
  FROM `bom_edge`
  WHERE `parent_id` = p_child_id AND `child_id` = p_parent_id;
  IF v_hit > 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'BOM 校验失败：A 与 B 已互为父子，会形成循环';
  END IF;

  -- 间接环：parent 是否已存在于 child 的子树里
  SET v_hit = 0;
  WITH RECURSIVE `sub` AS (
    SELECT p_child_id AS `id`, 1 AS `lvl`
    UNION ALL
    SELECT e.`child_id`, s.`lvl` + 1
    FROM `bom_edge` e
    JOIN `sub` s ON e.`parent_id` = s.`id`
    WHERE s.`lvl` < 20
  )
  SELECT COUNT(*) INTO v_hit FROM `sub` WHERE `sub`.`id` = p_parent_id;

  IF v_hit > 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'BOM 校验失败：该关系会形成循环包含';
  END IF;

  INSERT INTO `bom_edge` (`parent_id`, `child_id`, `quantity`, `seq_no`, `remark`)
  VALUES (p_parent_id, p_child_id, p_quantity, IFNULL(p_seq_no, 0), p_remark)
  ON DUPLICATE KEY UPDATE
    `quantity` = VALUES(`quantity`),
    `seq_no`   = VALUES(`seq_no`),
    `remark`   = VALUES(`remark`);
END$$

-- -----------------------------------------------------------------------------
-- 3. 存储过程：删除一个物料及其所有下属关系
--    元件被引用时禁止删除（宁可报错，也不要悄悄破坏别的产品的 BOM）。
-- -----------------------------------------------------------------------------
DROP PROCEDURE IF EXISTS `sp_item_delete`$$

CREATE PROCEDURE `sp_item_delete`(IN p_item_id BIGINT UNSIGNED)
COMMENT '软删除物料；若被其它物料引用则报错。调用方负责事务。'
BEGIN
  DECLARE v_used INT DEFAULT 0;
  DECLARE v_children INT DEFAULT 0;

  SELECT COUNT(*) INTO v_used FROM `bom_edge` WHERE `child_id` = p_item_id;
  IF v_used > 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = '删除失败：该物料仍被其它部件/产品引用';
  END IF;

  SELECT COUNT(*) INTO v_children FROM `bom_edge` WHERE `parent_id` = p_item_id;
  IF v_children > 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = '删除失败：请先清空该物料的子件';
  END IF;

  UPDATE `item` SET `status` = -1 WHERE `id` = p_item_id;
END$$

DELIMITER ;
