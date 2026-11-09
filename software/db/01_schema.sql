-- =============================================================================
--  电气公司报价查询系统 · 数据库结构脚本（01）
--  目标：MySQL 8.0.16+（需要 CHECK 约束、递归 CTE、JSON、nGram 全文索引、函数索引）
--  字符集：utf8mb4 / utf8mb4_0900_ai_ci
--  引擎：InnoDB
--  说明：本脚本可在开发环境重复执行（先 DROP 再 CREATE）
--        生产环境请勿直接执行 DROP 段，改用 04_migration 增量脚本
--  执行：mysql -u root -p --default-character-set=utf8mb4 < 01_schema.sql
-- =============================================================================

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

CREATE DATABASE IF NOT EXISTS `price_query`
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_0900_ai_ci;

USE `price_query`;

-- -----------------------------------------------------------------------------
-- 0. 清理（仅开发环境用）
-- -----------------------------------------------------------------------------
DROP VIEW  IF EXISTS `v_item_search`;
DROP VIEW  IF EXISTS `v_item_direct_price`;

DROP TABLE IF EXISTS `quotation_line`;
DROP TABLE IF EXISTS `quotation`;
DROP TABLE IF EXISTS `import_row`;
DROP TABLE IF EXISTS `import_batch`;
DROP TABLE IF EXISTS `price_record`;
DROP TABLE IF EXISTS `bom_edge`;
DROP TABLE IF EXISTS `item_alias`;
DROP TABLE IF EXISTS `item`;
DROP TABLE IF EXISTS `item_category`;
DROP TABLE IF EXISTS `audit_log`;
DROP TABLE IF EXISTS `app_setting`;
DROP TABLE IF EXISTS `schema_version`;

-- -----------------------------------------------------------------------------
-- 1. 物料主数据
--    元件 / 部件 / 产品共用一张表（单表继承），用 item_type 区分。
--    三者字段完全一致（编号、名称、单位、规格、分类、备注……），
--    差异只体现在「能否有子件」和「价格从哪里来」，属于行为差异而非结构差异。
-- -----------------------------------------------------------------------------
CREATE TABLE `item` (
  `id`          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '内部主键',
  `item_code`   VARCHAR(64)  NOT NULL COMMENT '物料编号（元件/部件/产品各自的编号，全局唯一）',
  `item_name`   VARCHAR(255) NOT NULL COMMENT '物料名称',
  `item_type`   ENUM('component','assembly','product') NOT NULL
                             COMMENT 'component=元件（不可再分） assembly=部件 product=产品',
  `spec`        VARCHAR(255) NULL COMMENT '规格型号',
  `unit`        VARCHAR(16)  NOT NULL DEFAULT '件' COMMENT '计量单位',
  `category_id` INT UNSIGNED NULL COMMENT '分类，便于筛选',
  `status`      TINYINT      NOT NULL DEFAULT 1 COMMENT '1=启用 0=停用 -1=已删除（软删除）',
  `remark`      VARCHAR(500) NULL COMMENT '备注',
  `created_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_item_code` (`item_code`),
  KEY `idx_item_type_status` (`item_type`, `status`),
  KEY `idx_item_name` (`item_name`),
  KEY `idx_item_category` (`category_id`),
  FULLTEXT KEY `ft_item_name_spec` (`item_name`, `spec`) WITH PARSER ngram
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='物料主表：元件/部件/产品';

-- 分类树（可选，够用即可；也可直接写成固定枚举）
CREATE TABLE `item_category` (
  `id`        INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `name`      VARCHAR(64)  NOT NULL,
  `parent_id` INT UNSIGNED NULL,
  `sort_no`   INT          NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_cat_name_parent` (`parent_id`, `name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='物料分类';

-- 别名 / 客户料号：Excel 里的编号往往不是内部编号，这一层是导入匹配成功率的關鍵
CREATE TABLE `item_alias` (
  `id`         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `item_id`    BIGINT UNSIGNED NOT NULL,
  `alias_code` VARCHAR(64)  NOT NULL COMMENT '外部编号（客户料号/旧ERP编号）',
  `alias_name` VARCHAR(255) NULL,
  `source`     VARCHAR(64)  NOT NULL DEFAULT '' COMMENT '来源标识：客户A / 旧ERP / 供应商X',
  `created_at` DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_alias` (`source`, `alias_code`),
  KEY `idx_alias_code` (`alias_code`),
  KEY `idx_alias_item` (`item_id`),
  CONSTRAINT `fk_alias_item` FOREIGN KEY (`item_id`) REFERENCES `item` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='物料别名/外部料号';

-- -----------------------------------------------------------------------------
-- 2. BOM：包含关系（邻接表）
--    一条边 = 父件里用了多少个某子件。
--    用量必须挂在「边」上而不是「件」上：同一个元件装在不同部件里用量不同。
--    层级深度只有 3~4 层，递归 CTE 足够，不必引入闭包表。
-- -----------------------------------------------------------------------------
CREATE TABLE `bom_edge` (
  `parent_id`  BIGINT UNSIGNED NOT NULL COMMENT '父件（部件/产品）',
  `child_id`   BIGINT UNSIGNED NOT NULL COMMENT '子件（元件/部件）',
  `quantity`   DECIMAL(18,4) NOT NULL DEFAULT 1.0000 COMMENT '单位用量',
  `unit`       VARCHAR(16)  NULL COMMENT '用量单位，一般与子件单位一致',
  `seq_no`     INT          NOT NULL DEFAULT 0 COMMENT '同级展示顺序',
  `remark`     VARCHAR(255) NULL,
  `created_at` DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`parent_id`, `child_id`),
  KEY `idx_edge_parent_seq` (`parent_id`, `seq_no`),
  KEY `idx_edge_child` (`child_id`),
  CONSTRAINT `fk_edge_parent` FOREIGN KEY (`parent_id`) REFERENCES `item` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fk_edge_child`  FOREIGN KEY (`child_id`)  REFERENCES `item` (`id`) ON DELETE RESTRICT,
  CONSTRAINT `ck_edge_qty` CHECK (`quantity` > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='BOM 包含关系（父件-子件-用量）';

-- -----------------------------------------------------------------------------
-- 3. 价格
--    price_kind 区分两种「价格」：
--      direct   —— 元件的录入/导入单价，是计算的唯一输入
--      reported —— 部件/产品在历史 Excel 中给出的成交总价，只用于校验，不参与计算
--    价格带 price_date 版本化：查询时取「该日期之前最新的一条」，
--    因此导入 2023 年的历史成交价不会污染当前报价。
-- -----------------------------------------------------------------------------
CREATE TABLE `price_record` (
  `id`            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `item_id`       BIGINT UNSIGNED NOT NULL,
  `price_kind`    ENUM('direct','reported') NOT NULL DEFAULT 'direct'
                                 COMMENT 'direct=录入单价 reported=历史成交总价(仅校验)',
  `amount`        DECIMAL(18,4) NOT NULL COMMENT '金额（direct 为单价，reported 为总价）',
  `currency`      CHAR(3)       NOT NULL DEFAULT 'CNY',
  `price_date`    DATE          NOT NULL COMMENT '价格生效日 / 成交日',
  `source`        ENUM('manual','excel','system') NOT NULL DEFAULT 'manual',
  `import_row_id` BIGINT UNSIGNED NULL COMMENT '来源于哪一行导入明细',
  `remark`        VARCHAR(255)  NULL,
  `created_by`    VARCHAR(64)   NULL COMMENT '录入人（Windows 账号）',
  `created_at`    DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  -- 这个唯一键同时承担「取某物料最新价」的查询：InnoDB 可以反向扫描索引，
  -- 因此 (item_id, price_kind, price_date) 已经能服务 ORDER BY price_date DESC，
  -- 不必再建一条重复索引。
  UNIQUE KEY `uk_price_item_kind_date` (`item_id`, `price_kind`, `price_date`),
  KEY `idx_price_date` (`price_date`),
  CONSTRAINT `fk_price_item` FOREIGN KEY (`item_id`) REFERENCES `item` (`id`) ON DELETE CASCADE,
  CONSTRAINT `ck_price_amount` CHECK (`amount` >= 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='价格版本表（元件录入价 + 部件/产品历史成交价）';

-- 当前有效价：每个元件取 price_date 最新的一条
CREATE OR REPLACE VIEW `v_item_direct_price` AS
SELECT r.`item_id`, r.`amount`, r.`currency`, r.`price_date`, r.`source`
FROM `price_record` r
WHERE r.`price_kind` = 'direct'
  AND r.`price_date` = (
        SELECT MAX(r2.`price_date`)
        FROM `price_record` r2
        WHERE r2.`item_id` = r.`item_id`
          AND r2.`price_kind` = 'direct'
      );

-- 查询用的宽视图：物料 + 当前单价（无价格时为 NULL）
CREATE OR REPLACE VIEW `v_item_search` AS
SELECT i.`id`           AS `item_id`,
       i.`item_code`,
       i.`item_name`,
       i.`item_type`,
       i.`spec`,
       i.`unit`,
       i.`category_id`,
       i.`status`,
       i.`remark`,
       i.`updated_at`,
       p.`amount`     AS `direct_price`,
       p.`price_date` AS `price_date`
FROM `item` i
LEFT JOIN `v_item_direct_price` p ON p.`item_id` = i.`id`
WHERE i.`status` <> -1;

-- -----------------------------------------------------------------------------
-- 4. 导入批次与导入明细
--    原始行（raw_json）全部落库：模板改了、匹配错了、事后要复盘，都有据可查。
-- -----------------------------------------------------------------------------
CREATE TABLE `import_batch` (
  `id`               BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `batch_type`       ENUM('history_price','quote_request') NOT NULL
                       COMMENT 'history_price=历史价格录入 quote_request=报价查询导入',
  `file_name`        VARCHAR(255) NOT NULL,
  `file_sha256`      CHAR(64)     NULL COMMENT '防重复导入',
  `template_version` VARCHAR(32)  NOT NULL DEFAULT 'v1',
  `total_rows`       INT          NOT NULL DEFAULT 0,
  `ok_rows`          INT          NOT NULL DEFAULT 0,
  `fail_rows`        INT          NOT NULL DEFAULT 0,
  `status`           ENUM('running','success','partial','failed') NOT NULL DEFAULT 'running',
  `message`          TEXT         NULL COMMENT '总体结论，如「3 行产品总价与明细和不符」',
  `created_by`       VARCHAR(64)  NULL,
  `created_at`       DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `finished_at`      DATETIME     NULL,
  PRIMARY KEY (`id`),
  KEY `idx_batch_type_time` (`batch_type`, `created_at` DESC),
  KEY `idx_batch_sha` (`file_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='Excel 导入批次';

CREATE TABLE `import_row` (
  `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `batch_id`        BIGINT UNSIGNED NOT NULL,
  `row_no`          INT           NOT NULL COMMENT 'Excel 行号，报错直接指给用户看',
  `raw_json`        JSON          NOT NULL COMMENT '原始单元格内容',
  `item_code`       VARCHAR(64)   NULL,
  `item_name`       VARCHAR(255)  NULL,
  `quantity`        DECIMAL(18,4) NULL,
  `amount`          DECIMAL(18,4) NULL COMMENT 'Excel 中的价格',
  `computed_amount` DECIMAL(18,4) NULL COMMENT '系统按明细算出的价格（校验用）',
  `matched_item_id` BIGINT UNSIGNED NULL,
  `match_type`      ENUM('exact','alias','fuzzy','none') NULL,
  `row_status`      ENUM('ok','warn','error') NOT NULL DEFAULT 'ok',
  `message`         VARCHAR(500)  NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_row_batch_no` (`batch_id`, `row_no`),
  KEY `idx_row_code` (`item_code`),
  KEY `idx_row_status` (`batch_id`, `row_status`),
  CONSTRAINT `fk_row_batch` FOREIGN KEY (`batch_id`) REFERENCES `import_batch` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fk_row_item`  FOREIGN KEY (`matched_item_id`) REFERENCES `item` (`id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='导入明细行';

-- -----------------------------------------------------------------------------
-- 5. 报价单（查询结果快照）
--    导入 Excel 查询或在线选型后生成一张报价单；
--    行是快照：单价、金额都冻结，日后改价不影响历史报价。
-- -----------------------------------------------------------------------------
CREATE TABLE `quotation` (
  `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `quote_no`        VARCHAR(32)   NOT NULL COMMENT '报价单号，如 Q20261106-001',
  `title`           VARCHAR(255)  NULL,
  `customer`        VARCHAR(128)  NULL,
  `quote_date`      DATE          NOT NULL,
  `price_date`      DATE          NULL COMMENT '计价基准日；NULL=使用当前最新价',
  `source`          ENUM('excel','search','selector') NOT NULL
                      COMMENT 'excel=导入查询 search=在线搜索 selector=在线选型',
  `total_amount`    DECIMAL(18,4) NOT NULL DEFAULT 0 COMMENT '合计（缺价为 0）',
  `priced_amount`   DECIMAL(18,4) NOT NULL DEFAULT 0 COMMENT '仅有效价格部分合计',
  `missing_count`   INT           NOT NULL DEFAULT 0 COMMENT '缺价条目数',
  `status`          ENUM('draft','final','archived') NOT NULL DEFAULT 'draft',
  `import_batch_id` BIGINT UNSIGNED NULL,
  `remark`          VARCHAR(500)  NULL,
  `created_by`      VARCHAR(64)   NULL,
  `created_at`      DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at`      DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_quote_no` (`quote_no`),
  KEY `idx_quote_date` (`quote_date` DESC),
  CONSTRAINT `fk_quote_batch` FOREIGN KEY (`import_batch_id`) REFERENCES `import_batch` (`id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='报价单';

CREATE TABLE `quotation_line` (
  `id`             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `quotation_id`   BIGINT UNSIGNED NOT NULL,
  `parent_line_id` BIGINT UNSIGNED NULL COMMENT '层级展示用；NULL=顶层（故意不加外键，避免自引用级联删除的顺序问题，由应用维护）',
  `item_id`        BIGINT UNSIGNED NULL COMMENT 'NULL=数据库中不存在该物料',
  `item_code`      VARCHAR(64)   NOT NULL COMMENT '快照：即使物料被删也能还原',
  `item_name`      VARCHAR(255)  NOT NULL,
  `item_type`      ENUM('component','assembly','product') NULL,
  `quantity`       DECIMAL(18,4) NOT NULL DEFAULT 1.0000 COMMENT '本级用量（相对父行）',
  `unit_price`     DECIMAL(18,4) NOT NULL DEFAULT 0 COMMENT '单价（缺价为 0）',
  `amount`         DECIMAL(18,4) NOT NULL DEFAULT 0 COMMENT 'quantity * unit_price',
  `price_state`    ENUM('priced','missing','zero','manual') NOT NULL DEFAULT 'priced'
                     COMMENT 'priced=有价 missing=库中无此物料/无价格 zero=价格为0 manual=手工改价',
  `line_source`    ENUM('input','expanded') NOT NULL DEFAULT 'input'
                     COMMENT 'input=Excel/用户给的原始行 expanded=BOM 展开出来的行',
  `selected`       TINYINT       NOT NULL DEFAULT 1 COMMENT '导出时是否勾选',
  `depth`          SMALLINT      NOT NULL DEFAULT 0,
  `sort_key`       VARCHAR(255)  NOT NULL DEFAULT '' COMMENT '展示排序键，如 001.002.003',
  `price_date`     DATE          NULL COMMENT '本行取用的价格日期',
  `remark`         VARCHAR(255)  NULL,
  PRIMARY KEY (`id`),
  KEY `idx_line_quote` (`quotation_id`, `sort_key`),
  KEY `idx_line_item` (`item_id`),
  KEY `idx_line_state` (`quotation_id`, `price_state`),
  CONSTRAINT `fk_line_quote` FOREIGN KEY (`quotation_id`) REFERENCES `quotation` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fk_line_item`  FOREIGN KEY (`item_id`) REFERENCES `item` (`id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='报价单明细（树形快照）';

-- -----------------------------------------------------------------------------
-- 6. 审计与配置
-- -----------------------------------------------------------------------------
CREATE TABLE `audit_log` (
  `id`          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `entity`      VARCHAR(32)   NOT NULL COMMENT 'item / bom_edge / price_record / quotation ...',
  `entity_id`   BIGINT UNSIGNED NULL,
  `action`      ENUM('create','update','delete','import','export','login') NOT NULL,
  `before_json` JSON          NULL,
  `after_json`  JSON          NULL,
  `operator`    VARCHAR(64)   NULL,
  `created_at`  DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_audit_entity` (`entity`, `entity_id`, `created_at` DESC),
  KEY `idx_audit_time` (`created_at` DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='操作审计';

CREATE TABLE `app_setting` (
  `k`          VARCHAR(64)  NOT NULL,
  `v`          VARCHAR(1024) NOT NULL,
  `updated_at` DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`k`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
  COMMENT='键值配置（含 schema_version、模板版本、缺价容差等）';

CREATE TABLE `schema_version` (
  `version`     INT          NOT NULL,
  `description` VARCHAR(255) NULL,
  `applied_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- price_record 建表时 import_row 还不存在（批次 → 明细 → 价格，顺序反过来会死锁），
-- 因此这条外键在建表后补上。
ALTER TABLE `price_record`
  ADD CONSTRAINT `fk_price_import_row`
  FOREIGN KEY (`import_row_id`) REFERENCES `import_row` (`id`) ON DELETE SET NULL;

INSERT INTO `schema_version` (`version`, `description`) VALUES (1, '初始版本');

SET FOREIGN_KEY_CHECKS = 1;
