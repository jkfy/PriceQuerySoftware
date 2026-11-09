#pragma once

#include <QAbstractItemModel>
#include <QColor>

struct QuoteNode;

class BomTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Column {
        ColName = 0, ColCode, ColType, ColQty, ColUnitPrice, ColAmount, ColState,
        ColumnCount
    };
    enum Roles {
        BadgeKindRole = Qt::UserRole + 1
    };
    enum BadgeKind {
        BadgeProduct = 0, BadgeAssembly, BadgeComponent,
        BadgeOk, BadgeMissing, BadgeWarning
    };

    explicit BomTreeModel(QObject *parent = nullptr);
    ~BomTreeModel() override;

    void setRoot(QuoteNode *root);            // 接管所有权
    QuoteNode *root() const { return m_root; }
    QuoteNode *nodeOf(const QModelIndex &idx) const;

    // QAbstractItemModel
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int         rowCount(const QModelIndex &parent = {}) const override;
    int         columnCount(const QModelIndex &parent = {}) const override;
    QVariant    data(const QModelIndex &index, int role) const override;
    bool        setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant    headerData(int section, Qt::Orientation o, int role) const override;

    // 业务辅助
    void    setAllChecked(bool checked);
    double  totalAmount() const;
    double  selectedAmount() const;
    int     missingCount() const;
    QuoteNode *findFirst(const QString &text) const;

    QStringList indexPath(const QModelIndex &idx) const;

signals:
    void checksChanged();

private:
    void emitSubtreeChanged(QuoteNode *n);
    void refreshAncestors(QuoteNode *n);

    QuoteNode *m_root = nullptr;
};
