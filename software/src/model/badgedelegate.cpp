#include "model/badgedelegate.h"
#include "model/bomtreemodel.h"

#include <QPainter>
#include <QApplication>
#include <QStyle>

namespace {
struct BadgeColors { QColor bg; QColor fg; };

BadgeColors colorsFor(int kind)
{
    switch (kind) {
    case BomTreeModel::BadgeProduct:   return { QColor("#e8f1fb"), QColor("#0a5cad") };
    case BomTreeModel::BadgeAssembly:  return { QColor("#e8f5e9"), QColor("#2e7d32") };
    case BomTreeModel::BadgeComponent: return { QColor("#f2f2f2"), QColor("#555555") };
    case BomTreeModel::BadgeOk:        return { QColor("#e8f5e9"), QColor("#2e7d32") };
    case BomTreeModel::BadgeMissing:   return { QColor("#fff3e0"), QColor("#b25d00") };
    case BomTreeModel::BadgeWarning:   return { QColor("#fff8e1"), QColor("#8a6d00") };
    default:                           return { QColor("#f2f2f2"), QColor("#555555") };
    }
}
} // namespace

BadgeDelegate::BadgeDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void BadgeDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
{
    // 先让样式画背景（含选中高亮），但不画文字
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear();

    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty())
        return;

    const BadgeColors c = colorsFor(index.data(BomTreeModel::BadgeKindRole).toInt());

    QFont f = option.font;
    if (f.pixelSize() > 0)
        f.setPixelSize(qMax(10, f.pixelSize() - 1));
    else if (f.pointSizeF() > 0)
        f.setPointSizeF(qMax(6.0, f.pointSizeF() - 0.5));
    QFontMetrics fm(f);
    const int tw = fm.horizontalAdvance(text);
    const int th = fm.height();
    const int w = tw + 12;
    const int h = qMin(th + 2, option.rect.height() - 6);

    QRect r(0, 0, w, h);
    r.moveCenter(option.rect.center());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(c.bg);
    painter->drawRoundedRect(r, 3, 3);
    painter->setPen(c.fg);
    painter->setFont(f);
    painter->drawText(r, Qt::AlignCenter, text);
    painter->restore();
}

QSize BadgeDelegate::sizeHint(const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setWidth(s.width() + 16);
    return s;
}
