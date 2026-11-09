#include "pages/quotationpage.h"
#include "ui_quotationpage.h"

#include "data/dataservice.h"
#include "model/bomtreemodel.h"
#include "model/badgedelegate.h"
#include "model/quotanode.h"

#include <QTreeView>
#include <QHeaderView>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QLocale>
#include <QMessageBox>
#include <QDate>
#include <QStandardItemModel>
#include <QTimer>
#include <QDebug>
#include <functional>

namespace {
QString money(double v)
{
    return QLocale(QLocale::Chinese, QLocale::China).toString(v, 'f', 2);
}
} // namespace

QuotationPage::QuotationPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::QuotationPage)
{
    ui->setupUi(this);

    ui->tabBar->addTab("导入 Excel 查询");
    ui->tabBar->addTab("在线搜索");
    ui->tabBar->addTab("在线选型");

    m_model = new BomTreeModel(this);
    setupTree();
    ui->splitter->setSizes({ 900, 268 });

    ui->metaLabel->setText(QString("客户：<b>华工电气</b>　计价基准日：<b>%1</b>")
                               .arg(QDate::currentDate().toString("yyyy-MM-dd")));

    connectSignals();
    reload();
}

QuotationPage::~QuotationPage()
{
    delete ui;
}

void QuotationPage::setupTree()
{
    ui->treeView->setModel(m_model);
    ui->treeView->setRootIsDecorated(true);
    ui->treeView->setUniformRowHeights(true);
    ui->treeView->setAllColumnsShowFocus(true);
    ui->treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->treeView->setAlternatingRowColors(false);

    QHeaderView *h = ui->treeView->header();
    h->setStretchLastSection(false);
    h->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeView->setColumnWidth(1, 140);
    ui->treeView->setColumnWidth(2, 100);
    ui->treeView->setColumnWidth(3, 88);
    ui->treeView->setColumnWidth(4, 120);
    ui->treeView->setColumnWidth(5, 130);
    ui->treeView->setColumnWidth(6, 110);

    ui->treeView->setItemDelegateForColumn(BomTreeModel::ColType, new BadgeDelegate(this));
    ui->treeView->setItemDelegateForColumn(BomTreeModel::ColState, new BadgeDelegate(this));
}

void QuotationPage::connectSignals()
{
    connect(ui->btnNewQuote,     &QPushButton::clicked, this, &QuotationPage::reload);
    connect(ui->btnImportExcel,  &QPushButton::clicked, this, &QuotationPage::requestImport);
    connect(ui->btnOnlineSelect, &QPushButton::clicked, this, &QuotationPage::requestImport);
    connect(ui->btnExpandAll,    &QPushButton::clicked, this, &QuotationPage::expandAll);
    connect(ui->btnCollapseAll,  &QPushButton::clicked, this, &QuotationPage::collapseAll);
    connect(ui->chipMissing,     &QPushButton::toggled, this, &QuotationPage::focusMissing);
    connect(ui->searchEdit,      &QLineEdit::returnPressed, this,
            [this] { search(ui->searchEdit->text()); });
    connect(m_model, &BomTreeModel::checksChanged, this, &QuotationPage::refreshSummary);

    connect(ui->btnExportSelected, &QPushButton::clicked, this,
            [this] { exportCsv("全部", false); });
    connect(ui->btnExportMissing, &QPushButton::clicked, this,
            [this] { exportCsv("缺价", true); });
    connect(ui->btnSaveQuotation, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "保存报价单",
            "草稿未接入写库；正式版将写入 quotation / quotation_line。");
    });
}

void QuotationPage::reload()
{
    QString err;
    QuoteNode *tree = DataService::instance().productTree(&err);
    if (!tree) {
        m_model->setRoot(nullptr);
        ui->countLabel->setText(err.isEmpty() ? "没有可展示的产品" : err);
        refreshSummary();
        return;
    }
    m_model->setRoot(tree);
    ui->treeView->expandToDepth(1);

    int total = 0;
    std::function<void(QuoteNode *)> count = [&](QuoteNode *n) {
        ++total;
        for (QuoteNode *c : n->children) count(c);
    };
    count(tree);
    ui->countLabel->setText(QString("共 %1 项 · 缺价 %2 项")
                                .arg(total).arg(m_model->missingCount()));
    refreshSummary();
}

void QuotationPage::refreshSummary()
{
    if (!m_model->root()) {
        ui->totalValue->setText("¥ 0.00");
        ui->totalSubLabel->setText("没有数据");
        ui->selectedValue->setText("¥ 0.00");
        ui->missingValue->setText("0");
        ui->dateValue->setText("—");
        ui->warnNote->hide();
        return;
    }

    const double total = m_model->totalAmount();
    const double sel   = m_model->selectedAmount();
    const int    miss  = m_model->missingCount();

    ui->totalValue->setText("¥ " + money(total));
    ui->totalSubLabel->setText(QString("其中 %1 项缺价，按 0 计入").arg(miss));
    ui->selectedValue->setText("¥ " + money(sel));
    ui->missingValue->setText(QString::number(miss));
    ui->missingValue->setStyleSheet(miss > 0
                                        ? "font-weight:600; color:#b25d00;"
                                        : "font-weight:600; color:#2e7d32;");
    ui->dateValue->setText(QDate::currentDate().toString("yyyy-MM-dd"));

    if (miss > 0) {
        QStringList codes;
        std::function<void(QuoteNode *)> walk = [&](QuoteNode *n) {
            if (n->isLeaf() && !n->hasPrice) codes << n->code;
            for (QuoteNode *c : n->children) walk(c);
        };
        walk(m_model->root());
        ui->warnNote->setText(QString("⚠ %1 在库中没有价格，已按 0 元计入，并标记为缺价。")
                                  .arg(codes.join("、")));
        ui->warnNote->show();
    } else {
        ui->warnNote->hide();
    }
}

void QuotationPage::expandAll()
{
    ui->treeView->expandAll();
}

void QuotationPage::collapseAll()
{
    ui->treeView->collapseAll();
    ui->treeView->expand(m_model->index(0, 0));
}

void QuotationPage::focusMissing(bool on)
{
    if (!on) {
        ui->treeView->expandAll();
        return;
    }
    ui->treeView->collapseAll();
    ui->treeView->expand(m_model->index(0, 0));

    std::function<void(QuoteNode *)> walk = [&](QuoteNode *n) {
        if (n->missingInSubtree) {
            QList<QuoteNode *> chain;
            for (QuoteNode *p = n; p; p = p->parent) chain.prepend(p);
            QModelIndex idx;
            for (QuoteNode *cur : chain) {
                const int row = cur->parent ? cur->parent->children.indexOf(cur) : 0;
                idx = m_model->index(row, 0, idx);
                ui->treeView->expand(idx);
            }
        }
        for (QuoteNode *c : n->children) walk(c);
    };
    walk(m_model->root());
}

void QuotationPage::search(const QString &text)
{
    QuoteNode *hit = m_model->findFirst(text.trimmed());
    if (!hit) {
        ui->countLabel->setText(QString("没有找到「%1」").arg(text));
        return;
    }
    QList<QuoteNode *> chain;
    for (QuoteNode *p = hit; p; p = p->parent) chain.prepend(p);
    QModelIndex idx;
    for (QuoteNode *cur : chain) {
        const int row = cur->parent ? cur->parent->children.indexOf(cur) : 0;
        idx = m_model->index(row, 0, idx);
        ui->treeView->expand(idx);
    }
    ui->treeView->setCurrentIndex(idx);
    ui->treeView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    ui->countLabel->setText(QString("已定位：%1 %2").arg(hit->code, hit->name));
}

void QuotationPage::exportCsv(const QString &suffix, bool onlyMissing)
{
    const QString path = QFileDialog::getSaveFileName(
        this, "导出报价结果",
        QString("报价结果_%1_%2.csv").arg(suffix, QDate::currentDate().toString("yyyyMMdd")),
        "CSV 文件 (*.csv)");
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", f.errorString());
        return;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << QChar(0xFEFF);                      // BOM：Excel 打开才不乱码
    ts << "名称,编号,类型,数量,单价,金额,状态\n";

    int rows = 0;
    std::function<void(QuoteNode *)> walk = [&](QuoteNode *n) {
        const bool isMissing = n->isLeaf() && !n->hasPrice;
        if (!onlyMissing || n->missingInSubtree) {
            ts << n->name << ','
               << n->code << ','
               << n->type << ','
               << n->qty << ','
               << (n->hasPrice ? money(n->unitPrice) : QString("—")) << ','
               << money(n->amount) << ','
               << (isMissing ? "缺价" : (n->missingInSubtree ? "含缺价" : "有价"))
               << "\n";
            ++rows;
        }
        for (QuoteNode *c : n->children) walk(c);
    };
    walk(m_model->root());
    f.close();

    QMessageBox::information(this, "导出完成",
                             QString("已导出 %1 行到：\n%2").arg(rows).arg(path));
}
