#include "model/bomtreemodel.h"
#include "model/quotanode.h"

#include <QLocale>
#include <QFont>
#include <QBrush>
#include <QStringList>
#include <functional>
#include <cmath>

namespace {
QString money(double v)
{
    return QLocale(QLocale::Chinese, QLocale::China).toString(v, 'f', 2);
}

QString qtyText(double v)
{
    if (std::fabs(v - std::floor(v)) < 1e-9 && std::fabs(v) < 1e12)
        return QString::number(static_cast<qint64>(v));
    return QString::number(v, 'f', 2);
}
} // namespace

BomTreeModel::BomTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

BomTreeModel::~BomTreeModel()
{
    delete m_root;
}

void BomTreeModel::setRoot(QuoteNode *root)
{
    beginResetModel();
    delete m_root;
    m_root = root;
    endResetModel();
    emit checksChanged();
}

QuoteNode *BomTreeModel::nodeOf(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<QuoteNode *>(idx.internalPointer()) : m_root;
}

QModelIndex BomTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!m_root || column < 0 || column >= ColumnCount || row < 0)
        return {};

    if (!parent.isValid())
        return row == 0 ? createIndex(0, column, m_root) : QModelIndex();

    QuoteNode *p = nodeOf(parent);
    if (!p || row >= p->children.size())
        return {};
    return createIndex(row, column, p->children.at(row));
}

QModelIndex BomTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || !m_root)
        return {};
    QuoteNode *n = nodeOf(child);
    if (!n || !n->parent || n == m_root)
        return {};
    QuoteNode *p = n->parent;
    if (p == m_root)
        return createIndex(0, 0, m_root);
    const int row = p->parent ? p->parent->children.indexOf(p) : 0;
    return createIndex(row, 0, p);
}

int BomTreeModel::rowCount(const QModelIndex &parent) const
{
    if (!m_root)
        return 0;
    if (!parent.isValid())
        return 1;
    QuoteNode *p = nodeOf(parent);
    return p ? p->children.size() : 0;
}

int BomTreeModel::columnCount(const QModelIndex &) const
{
    return ColumnCount;
}

QVariant BomTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    QuoteNode *n = nodeOf(index);
    if (!n)
        return {};

    const bool missing = n->isLeaf() && !n->hasPrice;

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColName:      return n->name;
        case ColCode:      return n->code;
        case ColType:      return n->type;
        case ColQty:       return qtyText(n->qty);
        case ColUnitPrice: return n->hasPrice ? money(n->unitPrice) : QString("—");
        case ColAmount:    return money(n->amount);
        case ColState:
            if (missing) return QString("缺价");
            if (n->missingInSubtree) return QString("含缺价");
            return QString("有价");
        default: break;
        }
        break;

    case Qt::CheckStateRole:
        if (index.column() == ColName)
            return n->check;
        break;

    case Qt::TextAlignmentRole:
        if (index.column() == ColQty || index.column() == ColUnitPrice
            || index.column() == ColAmount)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        if (index.column() == ColState)
            return int(Qt::AlignCenter);
        break;

    case Qt::ForegroundRole:
        if (missing)
            return QBrush(QColor("#9a9a9a"));
        if (index.column() == ColCode)
            return QBrush(QColor("#8a8a8a"));
        break;

    case Qt::FontRole:
        if (n->depth() == 0) {
            QFont f;
            f.setBold(true);
            return f;
        }
        break;

    case BadgeKindRole:
        if (index.column() == ColType) {
            if (n->type == "产品") return BadgeProduct;
            if (n->type == "部件") return BadgeAssembly;
            return BadgeComponent;
        }
        if (index.column() == ColState) {
            if (missing) return BadgeMissing;
            if (n->missingInSubtree) return BadgeWarning;
            return BadgeOk;
        }
        break;

    default:
        break;
    }
    return {};
}

bool BomTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::CheckStateRole || index.column() != ColName)
        return false;

    QuoteNode *n = nodeOf(index);
    if (!n)
        return false;

    const Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());

    std::function<void(QuoteNode *)> apply = [&](QuoteNode *node) {
        node->check = state;
        for (QuoteNode *c : node->children)
            apply(c);
    };
    apply(n);

    if (n->parent)
        refreshAncestors(n->parent);

    emitSubtreeChanged(m_root);
    emit checksChanged();
    return true;
}

void BomTreeModel::refreshAncestors(QuoteNode *n)
{
    while (n) {
        int checked = 0, partial = 0;
        for (QuoteNode *c : n->children) {
            if (c->check == Qt::Checked) ++checked;
            else if (c->check == Qt::PartiallyChecked) ++partial;
        }
        const int total = n->children.size();
        if (checked == total) n->check = Qt::Checked;
        else if (checked == 0 && partial == 0) n->check = Qt::Unchecked;
        else n->check = Qt::PartiallyChecked;
        n = n->parent;
    }
}

void BomTreeModel::emitSubtreeChanged(QuoteNode *n)
{
    if (!n) return;

    // 遍历子树，逐个节点通知（松散的树最多几百个节点，代价可接受）。
    QList<QuoteNode *> stack{ n };
    while (!stack.isEmpty()) {
        QuoteNode *cur = stack.takeLast();
        QModelIndex i;
        if (cur == m_root) {
            i = createIndex(0, 0, m_root);
        } else if (cur->parent) {
            const int row = cur->parent->children.indexOf(cur);
            i = createIndex(row, 0, cur);
        }
        if (i.isValid())
            emit dataChanged(i, createIndex(i.row(), ColumnCount - 1, cur),
                             { Qt::CheckStateRole, Qt::DisplayRole });
        for (QuoteNode *c : cur->children)
            stack << c;
    }
}

Qt::ItemFlags BomTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == ColName)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

QVariant BomTreeModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColName:      return QString("名称");
    case ColCode:      return QString("编号");
    case ColType:      return QString("类型");
    case ColQty:       return QString("数量");
    case ColUnitPrice: return QString("单价");
    case ColAmount:    return QString("金额");
    case ColState:     return QString("状态");
    default:           return {};
    }
}

void BomTreeModel::setAllChecked(bool checked)
{
    if (!m_root) return;
    const Qt::CheckState s = checked ? Qt::Checked : Qt::Unchecked;
    std::function<void(QuoteNode *)> apply = [&](QuoteNode *n) {
        n->check = s;
        for (QuoteNode *c : n->children) apply(c);
    };
    apply(m_root);
    emitSubtreeChanged(m_root);
    emit checksChanged();
}

double BomTreeModel::totalAmount() const
{
    return m_root ? m_root->amount : 0.0;
}

double BomTreeModel::selectedAmount() const
{
    if (!m_root) return 0.0;

    std::function<double(QuoteNode *)> sum = [&](QuoteNode *n) -> double {
        if (n->check == Qt::Unchecked) return 0.0;
        if (n->children.isEmpty())
            return n->check == Qt::Checked ? n->amount : 0.0;
        double s = 0.0;
        for (QuoteNode *c : n->children) s += sum(c);
        return s;
    };
    return sum(m_root);
}

int BomTreeModel::missingCount() const
{
    if (!m_root) return 0;
    int count = 0;
    std::function<void(QuoteNode *)> walk = [&](QuoteNode *n) {
        if (n->isLeaf() && !n->hasPrice) ++count;
        for (QuoteNode *c : n->children) walk(c);
    };
    walk(m_root);
    return count;
}

QuoteNode *BomTreeModel::findFirst(const QString &text) const
{
    if (!m_root || text.isEmpty()) return nullptr;
    QList<QuoteNode *> stack{ m_root };
    while (!stack.isEmpty()) {
        QuoteNode *n = stack.takeFirst();
        if (n->code.contains(text, Qt::CaseInsensitive)
            || n->name.contains(text, Qt::CaseInsensitive))
            return n;
        for (QuoteNode *c : n->children) stack << c;
    }
    return nullptr;
}

QStringList BomTreeModel::indexPath(const QModelIndex &idx) const
{
    QStringList out;
    QuoteNode *n = nodeOf(idx);
    while (n) {
        out.prepend(n->code);
        n = n->parent;
    }
    return out;
}
