# 软件开发说明

工程在 `software/`，设计文档在 `../softwareDesign/`。

---

## 1. 构建

```powershell
cmake --preset mingw-release
cmake --build --preset release
.\build\bin\PriceQuerySoftware.exe --demo
```

`CMakePresets.json` 里固定了本机工具链：

| 项 | 值 |
|---|---|
| Qt | `D:/QtCreate/6.10.0/mingw_64` |
| 编译器 | `D:/QtCreate/Tools/mingw1310_64/bin/g++.exe` |
| Ninja | `D:/QtCreate/Tools/Ninja/ninja.exe` |

`CMakeLists.txt` 开了 `AUTOMOC / AUTOUIC / AUTORCC` 和 `CMAKE_EXPORT_COMPILE_COMMANDS`，
`.ui` 在构建时由 `uic` 生成 `ui_*.h`。

---

## 2. 代码结构

```
src/
  main.cpp                 入口、命令行参数、--shot 截图模式
  theme.h                  全局 QSS + repolish()
  mainwindow.h/.cpp        主窗口
  forms/                   ★ 所有界面（.ui）
  data/dataservice.h/.cpp  SQL 访问层 + 演示数据，接口一致
  model/
    quotanode.h            报价树节点
    bomtreemodel.h/.cpp    QAbstractItemModel：多列、三态复选、灰显、汇总
    badgedelegate.h/.cpp   徽标绘制（圆角底色 + 居中文字）
  pages/
    quotationpage.h/.cpp   setupUi + 逻辑
    materialpage.h/.cpp
    otherpages.h/.cpp      价格 / 导入记录 / 设置
  importwizard.h/.cpp      4 步向导（页面内容来自 .ui）
```

一个页面的构造函数只有三类事：

```cpp
QuotationPage::QuotationPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::QuotationPage)
{
    ui->setupUi(this);
    m_model = new BomTreeModel(this);
    setupTree();          // 列宽、代理、展开策略
    connectSignals();     // 信号槽
    reload();             // 取数
}
```

---

## 3. 样式机制：动态属性 `kind`

`.ui` 里给控件加动态属性（Designer 属性面板底部「Dynamic Properties」）：

```xml
<property name="kind" stdset="0"><string>primary</string></property>
```

`theme.h` 里用属性选择器命中：

```css
QPushButton[kind="primary"] { background:#0a6ed1; color:#fff; }
QLabel[kind="muted"]        { color:#8a8a8a; font-size:11px; }
```

**为什么不用 objectName**：`.ui` 里控件的 `name` 就是 objectName，拿它当钩子会导致
「Designer 里改名 → 样式丢失」。动态属性可以随便加、可以多个控件共用同一个值。

可用的 `kind`：`primary` `chip` `link` `danger` `small` / `muted` `h1` `h2` `total`
`totalValue` `sumLabel` `tag` `warnNote` / `toolbar` `tabStrip` `filterBar` `sidebar`
`leftPane` `detailPane` / `summary` `card` `hline`。

---

## 4. 数据层

`DataService` 是单例，SQL 路径与演示路径**共用同一套接口**：

```cpp
QList<ItemRow>  items();
ItemRow         item(qint64 id);
QList<BomRow>   bom(qint64 parentId);
QList<PriceRow> prices(qint64 itemId);
QList<BatchRow> batches();
double          rollupPrice(qint64 itemId, int *missing);   // 递归汇总
QuoteNode      *productTree(QString *err);                  // BFS 展开 BOM
```

驱动按三级降级：`QMYSQL → QODBC → 演示数据`。ODBC 连接串会自动尝试多个 MySQL 驱动名。

> 草稿阶段 SQL 是字符串拼接（`sqlStr()` 做了转义）。正式版应全部改成
> `prepare + bindValue`。

---

## 5. 踩过的坑

**1. 动态属性选择器首次不生效**

`setupUi` 里设的 `kind`，控件首次 polish 时匹配不到规则（实测字体仍 12px，
手动 repolish 后变成 26px）。所以 `MainWindow::buildPages()` 和 `ImportWizard`
构造末尾都调用 `theme::repolish(this)`。

**2. 容器上 `setStyleSheet` 会污染子控件**

给工具栏设 `background:#fafafa`，里面的 `[kind="primary"]` 按钮就变白。
改为容器只加 `kind`，样式统一写在 `theme.h`。

**3. 顶部条被纵向拉伸**

`QVBoxLayout` 里普通 `QWidget` 默认 `Preferred` 策略，会分走剩余高度，
表现是工具栏极高、表格被压扁。`.ui` 里给这些容器设 `sizePolicy v=Maximum`，
给 `QSplitter` 设 `Expanding`。

**4. `QSortFilterProxyModel` 行号要 `mapToSource`**

列表里选中的是 proxy 行号，直接索引 `m_rows` 会错位。

**5. PowerShell 的 `Set-Content -Encoding UTF8` 会按 ANSI 重读**

用它改含中文的源码会把文件写坏（PS 5.1 的经典问题）。改文件一律用编辑器/写文件工具，
不要用 `Get-Content | Set-Content` 管道。

---

## 6. 开发辅助参数

```powershell
.\build\bin\PriceQuerySoftware.exe --demo --page 0 --shot out.png
#   --page 0 报价查询 / 1 物料 / 2 价格 / 3 导入记录 / 4 设置
.\build\bin\PriceQuerySoftware.exe --demo --wizard --wpage 2 --shot wizard.png
```

`--shot` 在 900ms 后 `grab()` 主窗口并退出，用来批量生成 `softwareDesign/ui/screenshots/`。

---

## 7. 下一步

1. 接 **QXlsx**：向导第 1 步读文件、第 2 步按列映射解析、第 3 步真做预检。
2. `DataService` 改 `prepare + bindValue`。
3. 写库补事务封装，导入整批一个事务。
4. 报价单保存 → `quotation` / `quotation_line` 快照。
