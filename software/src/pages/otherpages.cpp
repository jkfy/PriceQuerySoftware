#include "pages/otherpages.h"
#include "ui_pricepage.h"
#include "ui_importpage.h"
#include "ui_settingspage.h"

#include "model/badgedelegate.h"
#include "model/bomtreemodel.h"

#include <QTableView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QSettings>
#include <QLocale>
#include <QBrush>
#include <QSqlDatabase>

namespace {
QString money(double v)
{
    return QLocale(QLocale::Chinese, QLocale::China).toString(v, 'f', 2);
}
} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  PricePage
// ═══════════════════════════════════════════════════════════════════════
PricePage::PricePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PricePage)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({ "编号", "名称", "规格型号", "当前单价", "价格日期", "状态" });
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);

    ui->priceView->setModel(m_proxy);
    ui->priceView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->priceView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->priceView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->priceView->verticalHeader()->setVisible(false);
    ui->priceView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->priceView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->priceView->setItemDelegateForColumn(5, new BadgeDelegate(this));

    m_historyModel = new QStandardItemModel(this);
    m_historyModel->setHorizontalHeaderLabels({ "价格日期", "类型", "金额", "来源", "操作人" });
    ui->historyView->setModel(m_historyModel);
    ui->historyView->verticalHeader()->setVisible(false);
    ui->historyView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->historyView->horizontalHeader()->setStretchLastSection(true);

    connect(ui->searchEdit, &QLineEdit::textChanged, this,
            [this](const QString &t) { m_proxy->setFilterFixedString(t); });
    connect(ui->chipMissing, &QPushButton::toggled, this, [this](bool on) {
        m_onlyMissing = on;
        reload();
    });
    connect(ui->priceView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex &cur, const QModelIndex &) {
                if (!cur.isValid()) return;
                const int row = m_proxy->mapToSource(cur).row();
                if (row >= 0 && row < m_rows.size())
                    showPrices(m_rows.at(row).id);
            });

    reload();
}

PricePage::~PricePage()
{
    delete ui;
}

void PricePage::reload()
{
    m_rows = DataService::instance().components();
    m_model->removeRows(0, m_model->rowCount());

    int missing = 0;
    for (const ItemRow &r : m_rows) {
        if (!r.hasPrice) ++missing;
        if (m_onlyMissing && r.hasPrice) continue;

        auto *c0 = new QStandardItem(r.code);
        auto *c1 = new QStandardItem(r.name);
        auto *c2 = new QStandardItem(r.spec);
        auto *c3 = new QStandardItem(r.hasPrice ? money(r.price) : QString("—"));
        c3->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *c4 = new QStandardItem(r.priceDate.isEmpty() ? QString("—") : r.priceDate);
        auto *c5 = new QStandardItem(r.hasPrice ? "有价" : "缺价");
        c5->setData(r.hasPrice ? BomTreeModel::BadgeOk : BomTreeModel::BadgeMissing,
                    BomTreeModel::BadgeKindRole);
        c5->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        c0->setForeground(QColor("#666"));
        if (!r.hasPrice) {
            c3->setForeground(QColor("#9a9a9a"));
            c4->setForeground(QColor("#9a9a9a"));
        }
        m_model->appendRow({ c0, c1, c2, c3, c4, c5 });
    }

    ui->countLabel->setText(QString("共 %1 项 · 缺价 %2 项").arg(m_rows.size()).arg(missing));

    if (m_proxy->rowCount() > 0)
        ui->priceView->selectRow(0);
    else
        m_historyModel->removeRows(0, m_historyModel->rowCount());
}

void PricePage::showPrices(qint64 itemId)
{
    m_historyModel->removeRows(0, m_historyModel->rowCount());
    const QList<PriceRow> rows = DataService::instance().prices(itemId);
    for (const PriceRow &p : rows) {
        auto *c0 = new QStandardItem(p.date);
        auto *c1 = new QStandardItem(p.kind);
        auto *c2 = new QStandardItem(money(p.amount));
        c2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *c3 = new QStandardItem(p.source);
        auto *c4 = new QStandardItem(p.user);
        c4->setForeground(QColor("#8a8a8a"));
        m_historyModel->appendRow({ c0, c1, c2, c3, c4 });
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  ImportPage
// ═══════════════════════════════════════════════════════════════════════
ImportPage::ImportPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ImportPage)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels(
        { "导入时间", "类型", "文件名", "总行数", "成功", "失败", "状态" });

    ui->batchView->setModel(m_model);
    ui->batchView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->batchView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->batchView->verticalHeader()->setVisible(false);
    ui->batchView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->batchView->setItemDelegateForColumn(6, new BadgeDelegate(this));

    reload();
}

ImportPage::~ImportPage()
{
    delete ui;
}

void ImportPage::reload()
{
    m_model->removeRows(0, m_model->rowCount());
    const QList<BatchRow> rows = DataService::instance().batches();
    for (const BatchRow &b : rows) {
        auto *c0 = new QStandardItem(b.time);
        auto *c1 = new QStandardItem(b.type);
        auto *c2 = new QStandardItem(b.file);
        auto *c3 = new QStandardItem(b.total ? QString::number(b.total) : QString("—"));
        auto *c4 = new QStandardItem(b.total ? QString::number(b.ok) : QString("—"));
        auto *c5 = new QStandardItem(b.total ? QString::number(b.fail) : QString("—"));
        auto *c6 = new QStandardItem(b.status == "success" ? "成功"
                                    : b.status == "partial" ? "部分成功"
                                    : b.status == "failed"  ? "失败" : b.status);
        const int kind = b.status == "success" ? BomTreeModel::BadgeOk
                       : b.status == "partial" ? BomTreeModel::BadgeWarning
                                               : BomTreeModel::BadgeMissing;
        c6->setData(kind, BomTreeModel::BadgeKindRole);
        c6->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        for (QStandardItem *it : { c3, c4, c5 })
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_model->appendRow({ c0, c1, c2, c3, c4, c5, c6 });
    }
    ui->countLabel->setText(QString("共 %1 个批次").arg(rows.size()));
}

// ═══════════════════════════════════════════════════════════════════════
//  SettingsPage
// ═══════════════════════════════════════════════════════════════════════
SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsPage)
{
    ui->setupUi(this);

    const QStringList drivers = QSqlDatabase::drivers();
    ui->driverLabel->setText("可用驱动：" + (drivers.isEmpty() ? QString("无")
                                                               : drivers.join("、")));

    connect(ui->btnApply, &QPushButton::clicked, this, [this] {
        QSettings s;
        s.setValue("db/host", ui->hostEdit->text());
        s.setValue("db/port", ui->portEdit->text().toInt());
        s.setValue("db/name", ui->dbEdit->text());
        s.setValue("db/user", ui->userEdit->text());
        s.setValue("db/pwd", ui->pwdEdit->text());
        emit applyRequested(ui->hostEdit->text(), ui->portEdit->text().toInt(),
                            ui->dbEdit->text(), ui->userEdit->text(),
                            ui->pwdEdit->text());
    });
    connect(ui->btnUseDemo, &QPushButton::clicked, this, [this] {
        emit applyRequested(QString(), 0, QString(), QString(), QString());
    });
}

SettingsPage::~SettingsPage()
{
    delete ui;
}

void SettingsPage::loadFromSettings()
{
    QSettings s;
    ui->hostEdit->setText(s.value("db/host", "127.0.0.1").toString());
    ui->portEdit->setText(QString::number(s.value("db/port", 3306).toInt()));
    ui->dbEdit->setText(s.value("db/name", "price_query").toString());
    ui->userEdit->setText(s.value("db/user", "root").toString());
    ui->pwdEdit->setText(s.value("db/pwd", "").toString());
}

void SettingsPage::setStatusText(const QString &text, bool ok)
{
    ui->statusLabel->setText(text);
    ui->statusLabel->setStyleSheet(ok ? "color:#2e7d32;" : "color:#b25d00;");
}
