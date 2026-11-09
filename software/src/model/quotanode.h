#pragma once

#include <QString>
#include <QList>
#include <Qt>

// 报价树节点：内存中的一次查询结果。
// 与数据库表 item / bom_edge / price_record 对应，但展开后是一棵扁平化的树。
struct QuoteNode
{
    QString code;
    QString name;
    QString type;        // "产品" / "部件" / "元件"

    double  qty       = 1.0;
    double  unitPrice = 0.0;
    bool    hasPrice  = false;

    double  amount          = 0.0;   // 本节点对父节点的贡献额（已乘本节点用量）
    bool    missingInSubtree = false;

    Qt::CheckState check = Qt::Checked;

    QuoteNode *parent = nullptr;
    QList<QuoteNode *> children;

    ~QuoteNode() { qDeleteAll(children); }

    bool isLeaf() const { return children.isEmpty(); }
    int  depth() const
    {
        int d = 0;
        for (const QuoteNode *n = parent; n; n = n->parent) ++d;
        return d;
    }
};
