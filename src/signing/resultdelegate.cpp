// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "resultdelegate.h"

#include <QPainter>
#include <QStyleOptionViewItem>

void ResultDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Selection background
    if (option.state & QStyle::State_Selected)
        painter->fillRect(option.rect, option.palette.highlight());

    const QRect r = option.rect.adjusted(8, 4, -8, -4);
    const QString icon = index.data(IconTextRole).toString();
    const QColor iconColor(index.data(IconColorRole).toString());
    const QString message = index.data(MessageRole).toString();

    QFont f = option.font;
    f.setPointSize(10);
    painter->setFont(f);
    QFontMetrics fm(f);

    // Icon in color
    int iconW = fm.horizontalAdvance(icon + " ");
    painter->setPen(iconColor);
    painter->drawText(r.left(), r.top(), iconW, fm.height(), Qt::AlignLeft | Qt::AlignTop, icon);

    // Message with word wrap
    QRect textRect(r.left() + iconW, r.top(), r.width() - iconW, r.height());
    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, message);

    painter->restore();
}

QSize ResultDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QFont f = option.font;
    f.setPointSize(10);
    QFontMetrics fm(f);

    const QString icon = index.data(IconTextRole).toString();
    const QString message = index.data(MessageRole).toString();
    int iconW = fm.horizontalAdvance(icon + " ");

    int availableWidth = option.rect.width() > 0 ? option.rect.width() - 16 - iconW : 500;
    QRect textRect = fm.boundingRect(0, 0, availableWidth, 0, Qt::AlignLeft | Qt::TextWordWrap, message);

    int height = qMax(textRect.height(), fm.height()) + 8; // 4px top + 4px bottom padding
    return {option.rect.width(), height};
}
