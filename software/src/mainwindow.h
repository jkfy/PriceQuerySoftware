#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QLabel;
class QuotationPage;
class MaterialPage;
class PricePage;
class ImportPage;
class SettingsPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void reloadAll();
    void connectOrDemo(const QString &host, int port, const QString &db,
                       const QString &user, const QString &pwd, bool report);
    void setPage(int index);

private:
    void buildPages();
    void connectSignals();
    void openImportWizard();
    void updateStatusBar();

    Ui::MainWindow *ui;
    QLabel *m_statusLeft  = nullptr;
    QLabel *m_statusRight = nullptr;

    QuotationPage *m_quote    = nullptr;
    MaterialPage  *m_material = nullptr;
    PricePage     *m_price    = nullptr;
    ImportPage    *m_import   = nullptr;
    SettingsPage  *m_settings = nullptr;
};
