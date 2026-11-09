-- =============================================================================
--  电气公司报价查询系统 · 只读自检脚本（04）
--  用法：mysql -u root -p --default-character-set=utf8mb4 --table < 04_verify.sql
--  期望结果：
--    · 8 个物料、7 条 BOM 边、2 个触发器、2 个存储过程
--    · 展开 P-1001 得到 8 行，sort_key 形如 001.010.020
--    · 电气柜合计 981.00，线束 31.00，主控 950.00
--    · 缺价清单只有 C-3005
--    · 全文检索「端子」命中 C-3001
--  本脚本不含任何写操作，可随时重复执行。
-- =============================================================================

USE price_query;

SELECT '=== A. 对象与数据量 ===' AS s;
SELECT TABLE_NAME, TABLE_TYPE FROM information_schema.TABLES
WHERE TABLE_SCHEMA='price_query' ORDER BY TABLE_TYPE, TABLE_NAME;
SELECT TRIGGER_NAME FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='price_query';
SELECT ROUTINE_NAME FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='price_query';
SELECT (SELECT COUNT(*) FROM item)     AS item_count,
       (SELECT COUNT(*) FROM bom_edge) AS edge_count,
       (SELECT COUNT(*) FROM price_record) AS price_count;

SELECT '=== B. 当前有效价视图 ===' AS s;
SELECT * FROM v_item_direct_price ORDER BY item_id;

SELECT '=== C. 递归展开电气柜 P-1001 ===' AS s;
WITH RECURSIVE sub AS (
    SELECT i.id, i.item_code, i.item_name, i.item_type,
           CAST(NULL AS UNSIGNED)        AS parent_id,
           CAST(1 AS DECIMAL(18,4))      AS quantity,
           0                             AS depth,
           CAST('001' AS CHAR(64))       AS sort_key
    FROM item i WHERE i.id = 1
    UNION ALL
    SELECT c.id, c.item_code, c.item_name, c.item_type,
           e.parent_id, e.quantity, s.depth + 1,
           CONCAT(s.sort_key, '.', LPAD(e.seq_no, 3, '0'))
    FROM sub s
    JOIN bom_edge e ON e.parent_id = s.id
    JOIN item c     ON c.id = e.child_id
    WHERE s.depth < 20
)
SELECT s.depth, s.sort_key, s.item_code, s.item_name, s.quantity, p.amount AS unit_price
FROM sub s LEFT JOIN v_item_direct_price p ON p.item_id = s.id
ORDER BY s.sort_key;

SELECT '=== D. 自底向上汇总（路径前缀法）===' AS s;
WITH RECURSIVE sub AS (
    SELECT id, CAST('' AS CHAR(4000)) AS path, CAST(1 AS DECIMAL(28,8)) AS qty, 0 AS depth
    FROM item WHERE id = 1
    UNION ALL
    SELECT e.child_id, CONCAT(s.path, '/', e.child_id), s.qty * e.quantity, s.depth + 1
    FROM sub s JOIN bom_edge e ON e.parent_id = s.id
    WHERE s.depth < 20
),
leaf AS (
    SELECT s.id AS node_id, s.path, s.qty * IFNULL(p.amount, 0) AS amount
    FROM sub s LEFT JOIN v_item_direct_price p ON p.item_id = s.id
    WHERE NOT EXISTS (SELECT 1 FROM bom_edge e WHERE e.parent_id = s.id)
)
SELECT n.id, n.depth, SUM(l.amount) AS total
FROM sub n JOIN leaf l ON l.path LIKE CONCAT(n.path, '/%')
GROUP BY n.id, n.depth ORDER BY n.depth, n.id;

SELECT '=== E. 缺价清单 ===' AS s;
SELECT i.item_code, i.item_name, i.unit FROM item i
LEFT JOIN v_item_direct_price p ON p.item_id = i.id
WHERE i.status = 1 AND p.item_id IS NULL AND i.item_type='component'
ORDER BY i.item_code;

SELECT '=== F. 中文全文检索 ===' AS s;
SELECT item_code, item_name FROM item
WHERE MATCH(item_name, spec) AGAINST ('端子' IN BOOLEAN MODE) AND status = 1;

SELECT '=== G. ENUM 的排序是按定义顺序，不是字母序 ===' AS s;
SELECT item_type FROM item GROUP BY item_type ORDER BY item_type;

SELECT '=== H. 大小写不敏感（ai_ci）===' AS s;
SELECT item_code FROM item WHERE item_code = 'c-3001';

SELECT '=== I. 向上追溯：改 C-3001 的价格影响谁 ===' AS s;
WITH RECURSIVE up AS (
    SELECT parent_id, 1 AS lvl FROM bom_edge WHERE child_id = 4
    UNION ALL
    SELECT e.parent_id, u.lvl + 1 FROM bom_edge e JOIN up u ON e.child_id = u.parent_id
    WHERE u.lvl < 20
)
SELECT i.item_code, i.item_name, i.item_type FROM up JOIN item i ON i.id = up.parent_id;
