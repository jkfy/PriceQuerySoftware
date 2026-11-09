#pragma once

#include <QWidget>

#include "data/dataservice.h"

class QStandardItemModel;
class QSortFilterProxyModel;

QT_BEGIN_NAMESPACE
namespace Ui { class MaterialPage; }
QT_END_NAMESPACE

class MaterialPage : public QWidget
{
    Q_OBJECT
public:
    explicit MaterialPage(QWidget *parent = nullptr);
    ~MaterialPage() override;

    void reload();

private:
    void setupModels();
    void connectSignals();
    void applyFilter(const QString &text);
    void showItem(const ItemRow &row);
    void refreshBomTable(qint64 itemId);
    void refreshPriceTable(qint64 itemId);
    void updateTotalBar();

    Ui::MaterialPage *ui;
    QStandardItemModel   *m_listModel  = nullptr;
    QSortFilterProxyModel *m_proxy     = nullptr;
    QStandardItemModel   *m_bomModel   = nullptr;
    QStandardItemModel   *m_priceModel = nullptr;

    QList<ItemRow> m_items;
    ItemRow m_current;
};
