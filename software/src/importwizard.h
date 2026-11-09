#pragma once

#include <QWizard>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class WizardFilePage;
class WizardMappingPage;
class WizardPreviewPage;
class WizardDonePage;
}
QT_END_NAMESPACE

// 向导每一页的"内容"都是一个独立的 .ui 表单，
// 这样在 Qt Designer 里可以单独打开编辑，QWizard 只负责把它们串起来。
class WizardFilePage : public QWidget
{
    Q_OBJECT
public:
    explicit WizardFilePage(QWidget *parent = nullptr);
    ~WizardFilePage() override;
    Ui::WizardFilePage *ui;
};

class WizardMappingPage : public QWidget
{
    Q_OBJECT
public:
    explicit WizardMappingPage(QWidget *parent = nullptr);
    ~WizardMappingPage() override;
    Ui::WizardMappingPage *ui;
};

class WizardPreviewPage : public QWidget
{
    Q_OBJECT
public:
    explicit WizardPreviewPage(QWidget *parent = nullptr);
    ~WizardPreviewPage() override;
    Ui::WizardPreviewPage *ui;
};

class WizardDonePage : public QWidget
{
    Q_OBJECT
public:
    explicit WizardDonePage(QWidget *parent = nullptr);
    ~WizardDonePage() override;
    Ui::WizardDonePage *ui;
};

// 4 步导入向导
class ImportWizard : public QWizard
{
    Q_OBJECT
public:
    enum { PageFile = 0, PageMapping, PagePreview, PageDone };

    explicit ImportWizard(QWidget *parent = nullptr);
    ~ImportWizard() override;

private:
    QWizardPage *wrap(QWidget *content);
    void fillMappingCombos();
    void fillIssues();
    void connectPageSignals();

    WizardFilePage    *m_filePage    = nullptr;
    WizardMappingPage *m_mappingPage = nullptr;
    WizardPreviewPage *m_previewPage = nullptr;
    WizardDonePage    *m_donePage    = nullptr;
};
