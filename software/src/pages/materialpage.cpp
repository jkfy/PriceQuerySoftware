#include "pages/materialpage.h"
#include "ui_materialpage.h"

#include "model/badgedelegate.h"
#include "model/bomtreemodel.h"

#include <QTableView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QMessageBox>
#include <QLocale>
#include <QBrush>
#include <cmath>

namespace {
QString money(double v)
{
    return QLocale(QLocale::Chinese, QLocale::China).toString(v, 'f', 2);
}
QString qtyText(double v)
{
    if (std::abs(v - std::floor(v)) < 1e-9)
        return QString::number(static_cast<qint64>(v));
    return QString::number(v, 'f', 2);
}
} // namespace

MaterialPage::MaterialPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MaterialPage)
{
    ui->setupUi(this);

    setupModels();
    ui->splitter->setSizes({ 380, 900 });
    connectSignals();
    reload();
}

MaterialPage::~MaterialPage()
{
    delete ui;
}

void MaterialPage::setupModels()
{
    // 左侧物料列表
    m_listModel = new QStandardItemModel(this);
    m_listModel->setHorizontalHeaderLabels({ "编号", "名称", "类型" });

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_listModel);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);

    ui->listView->setModel(m_proxy);
    ui->listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->listView->verticalHeader()->setVisible(false);
    ui->listView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->listView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->listView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->listView->setShowGrid(false);
    ui->listView->setItemDelegateForColumn(2, new BadgeDelegate(this));

    // BOM 子件表
    m_bomModel = new QStandardItemModel(this);
    m_bomModel->setHorizontalHeaderLabels({ "编号", "名称", "类型", "用量", "单价", "金额" });
    ui->bomView->setModel(m_bomModel);
    ui->bomView->verticalHeader()->setVisible(false);
    ui->bomView->setEditTriggers(QAbstractItemView::DoubleClicked);
    ui->bomView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->bomView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->bomView->setItemDelegateForColumn(2, new BadgeDelegate(this));

    // 价格记录
    m_priceModel = new QStandardItemModel(this);
    m_priceModel->setHorizontalHeaderLabels({ "价格日期", "类型", "金额", "来源", "操作人" });
    ui->priceView->setModel(m_priceModel);
    ui->priceView->verticalHeader()->setVisible(false);
    ui->priceView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->priceView->horizontalHeader()->setStretchLastSection(true);
}

void MaterialPage::connectSignals()
{
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &MaterialPage::applyFilter);

    connect(ui->listView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex &cur, const QModelIndex &) {
                if (!cur.isValid()) return;
                const int row = m_proxy->mapToSource(cur).row();
                if (row >= 0 && row < m_items.size())
                    showItem(m_items.at(row));
            });

    auto explain = [this](const QString &title, const QString &body) {
        QMessageBox::information(this, title, body);
    };

    connect(ui->btnDeleteItem, &QPushButton::clicked, this, [this, explain] {
        explain("删除物料",
                QString("草稿未接入写库。\n正式版会调用 sp_item_delete(%1)，"
                        "被其它部件引用时会拒绝删除。").arg(m_current.id));
    });
    connect(ui->btnDeleteItemTop, &QPushButton::clicked, ui->btnDeleteItem, &QPushButton::click);
    connect(ui->btnEditBom, &QPushButton::clicked, this, [this, explain] {
        explain("修改 BOM",
                "正式版：表格内直接改用量、拖拽排序，"
                "点保存后一个事务内重写 bom_edge 并重算汇总。");
    });
    connect(ui->btnEditBomTop, &QPushButton::clicked, ui->btnEditBom, &QPushButton::click);
    connect(ui->btnEditInfo, &QPushButton::clicked, this, [this, explain] {
        explain("编辑信息", "正式版：切换成可编辑表单，保存时校验编号唯一性。");
    });
    connect(ui->btnNewItem, &QPushButton::clicked, this, [this, explain] {
        explain("新建物料", "正式版：打开新建对话框，编号查重后写入 item。");
    });
    connect(ui->btnCopyItem, &QPushButton::clicked, this, [this, explain] {
        explain("复制物料", "正式版：复制编号、名称与 BOM 结构，编号需重新输入。");
    });
    connect(ui->btnAddChild, &QPushButton::clicked, this, [this, explain] {
        explain("添加子件", "正式版：搜索物料 → 填写用量 → sp_bom_add_edge（含环检测）。");
    });
    connect(ui->btnRemoveChild, &QPushButton::clicked, this, [this, explain] {
        explain("移除子件", "正式版：删除 bom_edge 中对应的一行。");
    });
}

void MaterialPage::reload()
{
    m_items = DataService::instance().items();

    m_listModel->removeRows(0, m_listModel->rowCount());
    for (const ItemRow &r : m_items) {
        auto *c0 = new QStandardItem(r.code);
        auto *c1 = new QStandardItem(r.name);
        auto *c2 = new QStandardItem(r.type);
        c2->setData(r.type == "产品" ? BomTreeModel::BadgeProduct
                      : r.type == "部件" ? BomTreeModel::BadgeAssembly
                                         : BomTreeModel::BadgeComponent,
                    BomTreeModel::BadgeKindRole);
        c2->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        c0->setForeground(QColor("#666"));
        m_listModel->appendRow({ c0, c1, c2 });
    }
    ui->listCount->setText(QString("共 %1").arg(m_items.size()));

    if (!m_items.isEmpty())
        ui->listView->selectRow(0);
}

void MaterialPage::applyFilter(const QString &text)
{
    m_proxy->setFilterFixedString(text);
    if (m_proxy->rowCount() > 0 && !ui->listView->currentIndex().isValid())
        ui->listView->selectRow(0);
}

void MaterialPage::showItem(const ItemRow &row)
{
    m_current = row;

    ui->detailTitle->setText(row.name);
    ui->detailCode->setText(QString("%1 · %2").arg(row.code, row.type));
    ui->specValue->setText(row.spec.isEmpty() ? "—" : row.spec);
    ui->unitValue->setText(row.unit.isEmpty() ? "—" : row.unit);
    ui->categoryValue->setText(row.type == "元件" ? "电气 / 元件" : "电气 / " + row.type);
    ui->statusValue->setText("启用");

    refreshBomTable(row.id);
    refreshPriceTable(row.id);
    updateTotalBar();
}

void MaterialPage::refreshBomTable(qint64 itemId)
{
    m_bomModel->removeRows(0, m_bomModel->rowCount());
    const QList<BomRow> rows = DataService::instance().bom(itemId);

    for (const BomRow &b : rows) {
        // 子件可能是部件，单价要用它自己的子树汇总价
        int miss = 0;
        const double childPrice = DataService::instance().rollupPrice(b.id, &miss);
        const double amount = b.qty * childPrice;
        const bool ok = (miss == 0);

        auto *c0 = new QStandardItem(b.code);
        auto *c1 = new QStandardItem(b.name);
        auto *c2 = new QStandardItem(b.type);
        c2->setData(b.type == "部件" ? BomTreeModel::BadgeAssembly
                                     : BomTreeModel::BadgeComponent,
                    BomTreeModel::BadgeKindRole);
        c2->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        auto *c3 = new QStandardItem(qtyText(b.qty));
        auto *c4 = new QStandardItem(ok ? money(childPrice) : QString("—"));
        auto *c5 = new QStandardItem(money(amount));
        for (QStandardItem *it : { c3, c4, c5 })
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (!ok) {
            c4->setForeground(QColor("#9a9a9a"));
            c5->setForeground(QColor("#9a9a9a"));
        }
        m_bomModel->appendRow({ c0, c1, c2, c3, c4, c5 });
    }
    ui->bomCount->setText(rows.isEmpty() ? QString("（元件无子件）")
                                         : QString("%1 个子件").arg(rows.size()));
}

void MaterialPage::refreshPriceTable(qint64 itemId)
{
    m_priceModel->removeRows(0, m_priceModel->rowCount());
    const QList<PriceRow> rows = DataService::instance().prices(itemId);
    for (const PriceRow &p : rows) {
        auto *c0 = new QStandardItem(p.date);
        auto *c1 = new QStandardItem(p.kind);
        auto *c2 = new QStandardItem(money(p.amount));
        c2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *c3 = new QStandardItem(p.source);
        auto *c4 = new QStandardItem(p.user);
        c4->setForeground(QColor("#8a8a8a"));
        m_priceModel->appendRow({ c0, c1, c2, c3, c4 });
    }
    if (rows.isEmpty()) {
        auto *c0 = new QStandardItem("—");
        auto *c1 = new QStandardItem("没有价格记录");
        c1->setForeground(QColor("#9a9a9a"));
        m_priceModel->appendRow({ c0, c1, new QStandardItem(""),
                                  new QStandardItem(""), new QStandardItem("") });
    }
}

void MaterialPage::updateTotalBar()
{
    if (m_current.type == "元件") {
        ui->totalValue->setText(m_current.hasPrice ? "¥ " + money(m_current.price) : "缺价");
        ui->totalValue->setStyleSheet(m_current.hasPrice
                                          ? "font-size:17px; font-weight:600;"
                                          : "font-size:17px; font-weight:600; color:#b25d00;");
        ui->totalCaption->setText("元件价格");
        ui->priceHint->setText("元件价格由人工录入，可在此修改");
    } else {
        int miss = 0;
        const double total = DataService::instance().rollupPrice(m_current.id, &miss);
        ui->totalValue->setText("¥ " + money(total));
        ui->totalValue->setStyleSheet("font-size:17px; font-weight:600;");
        ui->totalCaption->setText("部件价格");
        ui->priceHint->setText(miss > 0
                                   ? QString("含 %1 项缺价，按 0 计入").arg(miss)
                                   : QString("部件 / 产品的价格由子件金额加和得出，不可直接编辑"));
    }
}
