#pragma once

#include <QWidget>

#include "data/dataservice.h"

class QStandardItemModel;
class QSortFilterProxyModel;

QT_BEGIN_NAMESPACE
namespace Ui {
class PricePage;
class ImportPage;
class SettingsPage;
}
QT_END_NAMESPACE

// ── 价格管理 ────────────────────────────────────────────────────────────
class PricePage : public QWidget
{
    Q_OBJECT
public:
    explicit PricePage(QWidget *parent = nullptr);
    ~PricePage() override;

    void reload();

private:
    void showPrices(qint64 itemId);

    Ui::PricePage *ui;
    QStandardItemModel    *m_model        = nullptr;
    QSortFilterProxyModel *m_proxy        = nullptr;
    QStandardItemModel    *m_historyModel = nullptr;
    QList<ItemRow> m_rows;
    bool m_onlyMissing = false;
};

// ── 导入记录 ────────────────────────────────────────────────────────────
class ImportPage : public QWidget
{
    Q_OBJECT
public:
    explicit ImportPage(QWidget *parent = nullptr);
    ~ImportPage() override;

    void reload();

private:
    Ui::ImportPage *ui;
    QStandardItemModel *m_model = nullptr;
};

// ── 设置 ────────────────────────────────────────────────────────────────
class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage() override;

    void loadFromSettings();
    void setStatusText(const QString &text, bool ok);

signals:
    void applyRequested(const QString &host, int port, const QString &db,
                        const QString &user, const QString &pwd);

private:
    Ui::SettingsPage *ui;
};
