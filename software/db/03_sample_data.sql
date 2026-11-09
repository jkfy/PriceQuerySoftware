-- =============================================================================
--  电气公司报价查询系统 · 演示数据（03）
--  用途：跑通「导入 → 查询 → 展开 → 导出」的最短验证路径
--  结构：1 个产品 → 2 个部件 → 5 个元件（其中 1 个故意不录价格，用于验证缺价提示）
--
--  期望计算结果：
--    线束组件 A-2001 = 10 × 2.50 + 5 × 1.20        =  31.00
--    主控模块 A-2002 = 1 × 800.00 + 1 × 150.00     = 950.00
--    电气柜   P-1001 = 31.00 + 950.00 + 2 × 缺价   = 981.00（缺价按 0，并标记）
-- =============================================================================

USE `price_query`;
SET NAMES utf8mb4;

INSERT INTO `item` (`id`, `item_code`, `item_name`, `item_type`, `spec`, `unit`, `remark`) VALUES
  (1, 'P-1001', '电气柜 A 型',  'product',   '800×600×2000', '台', '示例产品'),
  (2, 'A-2001', '线束组件',     'assembly',  '主线束',       '套', NULL),
  (3, 'A-2002', '主控模块',     'assembly',  '含控制器+显示','套', NULL),
  (4, 'C-3001', '接线端子',     'component', 'UK-2.5',       '个', NULL),
  (5, 'C-3002', '导线',         'component', 'RV 1.0',       '米', NULL),
  (6, 'C-3003', '控制器',       'component', 'PLC-24DI',     '个', NULL),
  (7, 'C-3004', '显示屏',       'component', '7寸触摸',      '个', NULL),
  (8, 'C-3005', '保险丝',       'component', '5×20 2A',      '个', '故意不录价格，用于验证缺价提示');

INSERT INTO `bom_edge` (`parent_id`, `child_id`, `quantity`, `seq_no`) VALUES
  (1, 2,  1.0000, 10),
  (1, 3,  1.0000, 20),
  (1, 8,  2.0000, 30),
  (2, 4, 10.0000, 10),
  (2, 5,  5.0000, 20),
  (3, 6,  1.0000, 10),
  (3, 7,  1.0000, 20);

-- 元件当前单价（direct）
INSERT INTO `price_record` (`item_id`, `price_kind`, `amount`, `price_date`, `source`) VALUES
  (4, 'direct',   2.5000, '2026-01-01', 'manual'),
  (5, 'direct',   1.2000, '2026-01-01', 'manual'),
  (6, 'direct', 800.0000, '2026-01-01', 'manual'),
  (7, 'direct', 150.0000, '2026-01-01', 'manual');

-- 历史成交价（reported）：只做「总价 vs 明细和」的一致性校验，不参与计算
INSERT INTO `price_record` (`item_id`, `price_kind`, `amount`, `price_date`, `source`, `remark`) VALUES
  (1, 'reported',  981.0000, '2026-01-15', 'excel', '示例：与明细和一致'),
  (2, 'reported',   31.0000, '2026-01-15', 'excel', '示例：与明细和一致'),
  (3, 'reported',  950.0000, '2026-01-15', 'excel', '示例：与明细和一致');

-- 客户料号示例：Excel 里可能用的是客户编号
INSERT INTO `item_alias` (`item_id`, `alias_code`, `alias_name`, `source`) VALUES
  (4, 'KH-DZ-001', '端子(客户编号)', '客户A'),
  (6, 'KH-PLC-24', '控制器(客户编号)', '客户A');
