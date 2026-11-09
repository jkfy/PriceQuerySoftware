#include "importwizard.h"
#include "theme.h"

#include "ui_wizardfilepage.h"
#include "ui_wizardmappingpage.h"
#include "ui_wizardpreviewpage.h"
#include "ui_wizarddonepage.h"

#include <QWizardPage>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QTableWidget>

// ═══════════════════════════════════════════════════════════════════════
//  四个页面内容（纯 .ui）
// ═══════════════════════════════════════════════════════════════════════
WizardFilePage::WizardFilePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WizardFilePage)
{
    ui->setupUi(this);
}

WizardFilePage::~WizardFilePage()
{
    delete ui;
}

WizardMappingPage::WizardMappingPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WizardMappingPage)
{
    ui->setupUi(this);
}

WizardMappingPage::~WizardMappingPage()
{
    delete ui;
}

WizardPreviewPage::WizardPreviewPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WizardPreviewPage)
{
    ui->setupUi(this);
}

WizardPreviewPage::~WizardPreviewPage()
{
    delete ui;
}

WizardDonePage::WizardDonePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WizardDonePage)
{
    ui->setupUi(this);
}

WizardDonePage::~WizardDonePage()
{
    delete ui;
}

// ═══════════════════════════════════════════════════════════════════════
//  ImportWizard
// ═══════════════════════════════════════════════════════════════════════
ImportWizard::ImportWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle("导入 Excel");
    setWizardStyle(QWizard::ClassicStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setOption(QWizard::HaveHelpButton, false);
    resize(1080, 700);

    m_filePage    = new WizardFilePage;
    m_mappingPage = new WizardMappingPage;
    m_previewPage = new WizardPreviewPage;
    m_donePage    = new WizardDonePage;

    setPage(PageFile,    wrap(m_filePage));
    setPage(PageMapping, wrap(m_mappingPage));
    setPage(PagePreview, wrap(m_previewPage));
    setPage(PageDone,    wrap(m_donePage));

    setButtonText(QWizard::NextButton, "下一步");
    setButtonText(QWizard::BackButton, "上一步");
    setButtonText(QWizard::CancelButton, "取消");
    setButtonText(QWizard::FinishButton, "完成");

    fillMappingCombos();
    fillIssues();
    connectPageSignals();

    theme::repolish(this);
}

ImportWizard::~ImportWizard() = default;

QWizardPage *ImportWizard::wrap(QWidget *content)
{
    auto *page = new QWizardPage;
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(content);
    return page;
}

void ImportWizard::fillMappingCombos()
{
    const QStringList fields{ "（忽略）", "编号", "名称", "数量", "单价", "总价", "类型", "备注" };
    const QStringList presets{ "编号", "名称", "数量", "总价", "备注" };
    const QList<QComboBox *> combos{ m_mappingPage->ui->comboA, m_mappingPage->ui->comboB,
                                     m_mappingPage->ui->comboC, m_mappingPage->ui->comboD,
                                     m_mappingPage->ui->comboE };
    for (int i = 0; i < combos.size(); ++i) {
        combos.at(i)->addItems(fields);
        combos.at(i)->setCurrentText(presets.at(i));
    }
}

void ImportWizard::fillIssues()
{
    auto *table = m_previewPage->ui->issueTable;
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels({ "行号", "编号", "名称", "类型", "问题", "说明" });
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setShowGrid(false);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->setColumnWidth(0, 64);
    table->setColumnWidth(1, 110);
    table->setColumnWidth(2, 150);
    table->setColumnWidth(3, 70);
    table->setColumnWidth(4, 76);

    struct Issue { QString row, code, name, type, kind, desc; };
    const QList<Issue> issues = {
        { "52",  "P-2003", "配电柜 B 型",  "产品", "错误", "产品总价与明细不符：Excel 1,240.00 ≠ 系统 1,286.50（差 46.50）" },
        { "88",  "X-9001", "未知料号",    "—",    "错误", "数据库中不存在该编号，且无别名匹配" },
        { "37",  "C-3005", "保险丝",      "元件", "缺价", "库中无该元件价格，导入后按 0 计算并标记为缺价" },
        { "103", "A-2001", "线束组件",    "部件", "警告", "数量为空，已按 1 处理" },
        { "214", "C-3011", "断路器",      "元件", "警告", "本批次重复出现 2 次，将合并（用量相加）" },
        { "402", "A-2010", "端子排组件",  "部件", "缺价", "该部件下有 2 个元件缺价，无法校验总价" },
        { "517", "P-1001", "电气柜 A 型", "产品", "警告", "总价与明细差 0.02，在容差内，按系统值写入" },
        { "903", "C-3002", "导线",        "元件", "缺价", "库中无该元件价格，导入后按 0 计算并标记为缺价" },
    };

    table->setRowCount(issues.size());
    for (int i = 0; i < issues.size(); ++i) {
        const Issue &is = issues.at(i);
        table->setItem(i, 0, new QTableWidgetItem(is.row));
        table->setItem(i, 1, new QTableWidgetItem(is.code));
        table->setItem(i, 2, new QTableWidgetItem(is.name));
        table->setItem(i, 3, new QTableWidgetItem(is.type));
        auto *kind = new QTableWidgetItem(is.kind);
        kind->setTextAlignment(Qt::AlignCenter);
        if (is.kind == "错误")      kind->setForeground(QColor("#c62828"));
        else if (is.kind == "警告") kind->setForeground(QColor("#8a6d00"));
        else                        kind->setForeground(QColor("#b25d00"));
        table->setItem(i, 4, kind);
        table->setItem(i, 5, new QTableWidgetItem(is.desc));
        table->setRowHeight(i, 30);
    }
}

void ImportWizard::connectPageSignals()
{
    connect(m_filePage->ui->btnBrowse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(
            this, "选择 Excel 文件", QString(), "Excel 文件 (*.xlsx *.xls)");
        if (!f.isEmpty())
            m_filePage->ui->fileNameEdit->setText(f);
    });

    connect(m_previewPage->ui->btnExportIssues, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "导出问题清单",
                                 "草稿未实现文件导出；正式版会导出为 xlsx 并附带原始行。");
    });
}
