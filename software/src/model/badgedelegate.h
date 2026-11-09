#pragma once

#include <QStyledItemDelegate>

// 在表格/树里画"徽标"：圆角底色 + 居中文字。
// 颜色由 model 的 BadgeKindRole 决定，具体映射见 .cpp。
class BadgeDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit BadgeDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};
