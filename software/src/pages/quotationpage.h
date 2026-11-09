#pragma once

#include <QWidget>

class BomTreeModel;

QT_BEGIN_NAMESPACE
namespace Ui { class QuotationPage; }
QT_END_NAMESPACE

class QuotationPage : public QWidget
{
    Q_OBJECT
public:
    explicit QuotationPage(QWidget *parent = nullptr);
    ~QuotationPage() override;

    void reload();

signals:
    void requestImport();

private:
    void setupTree();
    void connectSignals();
    void refreshSummary();
    void expandAll();
    void collapseAll();
    void focusMissing(bool on);
    void search(const QString &text);
    void exportCsv(const QString &suffix, bool onlyMissing);

    Ui::QuotationPage *ui;
    BomTreeModel *m_model = nullptr;
};
