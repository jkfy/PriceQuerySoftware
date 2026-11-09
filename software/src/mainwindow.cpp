#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "pages/quotationpage.h"
#include "pages/materialpage.h"
#include "pages/otherpages.h"
#include "importwizard.h"
#include "data/dataservice.h"
#include "theme.h"

#include <QLabel>
#include <QStatusBar>
#include <QSettings>
#include <QDateTime>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setMinimumSize(1200, 760);

    buildPages();
    connectSignals();

    // 启动时按 QSettings 里保存的参数尝试连接，失败则静默降级为演示数据
    QSettings s;
    connectOrDemo(s.value("db/host", "127.0.0.1").toString(),
                  s.value("db/port", 3306).toInt(),
                  s.value("db/name", "price_query").toString(),
                  s.value("db/user", "root").toString(),
                  s.value("db/pwd", "").toString(),
                  false);

    m_settings->loadFromSettings();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::buildPages()
{
    // 导航项
    ui->navList->addItem("▤   报价查询");
    ui->navList->addItem("▦   物料管理");
    ui->navList->addItem("¥   价格管理");
    ui->navList->addItem("⇩   导入记录");
    ui->navList->addItem("⚙   设置");
    ui->navList->setCurrentRow(0);

    // 页面：自定义控件在代码里挂进 stack，
    // 这样 .ui 只负责外壳，页面本身各自有独立的 .ui 文件
    m_quote    = new QuotationPage;
    m_material = new MaterialPage;
    m_price    = new PricePage;
    m_import   = new ImportPage;
    m_settings = new SettingsPage;

    ui->stack->addWidget(m_quote);
    ui->stack->addWidget(m_material);
    ui->stack->addWidget(m_price);
    ui->stack->addWidget(m_import);
    ui->stack->addWidget(m_settings);

    // 状态栏两个标签
    m_statusLeft  = new QLabel;
    m_statusRight = new QLabel;
    ui->statusBar->addWidget(m_statusLeft, 1);
    ui->statusBar->addPermanentWidget(m_statusRight);
    updateStatusBar();

    // .ui 里的 kind 动态属性需要重刷一次样式才生效（见 theme.h 说明）
    theme::repolish(this);
}

void MainWindow::connectSignals()
{
    connect(ui->navList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < ui->stack->count())
            ui->stack->setCurrentIndex(row);
    });

    connect(m_quote, &QuotationPage::requestImport, this, &MainWindow::openImportWizard);

    connect(m_settings, &SettingsPage::applyRequested, this,
            [this](const QString &host, int port, const QString &db,
                   const QString &user, const QString &pwd) {
                if (host.isEmpty()) {          // "使用演示数据"
                    DataService::instance().useDemo();
                    reloadAll();
                    updateStatusBar();
                    m_settings->setStatusText("已切换为演示数据", false);
                    return;
                }
                connectOrDemo(host, port, db, user, pwd, true);
            });
}

void MainWindow::openImportWizard()
{
    ImportWizard wizard(this);
    wizard.exec();
}

void MainWindow::connectOrDemo(const QString &host, int port, const QString &db,
                               const QString &user, const QString &pwd, bool report)
{
    QString err;
    if (DataService::instance().connectDb(host, port, db, user, pwd, &err)) {
        m_settings->setStatusText("连接成功：" + DataService::instance().statusText(), true);
    } else {
        DataService::instance().useDemo();
        m_settings->setStatusText("连接失败，已降级为演示数据。\n" + err, false);
        if (report)
            QMessageBox::warning(this, "数据库连接失败",
                                 err + "\n\n程序已切换为演示数据，界面仍可正常浏览。");
    }
    reloadAll();
    updateStatusBar();
}

void MainWindow::reloadAll()
{
    m_quote->reload();
    m_material->reload();
    m_price->reload();
    m_import->reload();
}

void MainWindow::setPage(int index)
{
    if (index < 0 || index >= ui->stack->count())
        return;
    ui->navList->setCurrentRow(index);
    ui->stack->setCurrentIndex(index);
}

void MainWindow::updateStatusBar()
{
    const bool ok = DataService::instance().isConnected();
    const QString dot = ok ? "#35a854" : "#e8a33d";
    m_statusLeft->setText(
        QString("<span style='color:%1'>●</span>&nbsp;%2")
            .arg(dot, DataService::instance().statusText()));
    m_statusRight->setText("最后刷新 " + QDateTime::currentDateTime().toString("HH:mm:ss"));
}
