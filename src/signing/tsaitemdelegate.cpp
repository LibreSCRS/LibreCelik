// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "tsaitemdelegate.h"

#include "signingcolors.h"

#include <QPainter>

TsaItemDelegate::TsaItemDelegate(QAbstractItemView* view, QObject* parent) : ListItemDelegate(view, parent) {}

void TsaItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QRect rect = option.rect;
    const QString type = index.data(Qt::UserRole).toString();
    const bool isAdd = (type == QStringLiteral("add"));
    const bool isCustom = (type == QStringLiteral("custom"));
    const bool hovered = option.state & QStyle::State_MouseOver;

    if (hovered) {
        QColor bg = signing::kTealColor;
        bg.setAlpha(isAdd ? 25 : 40);
        painter->fillRect(rect, bg);
    }

    if (isAdd) {
        QFont font = option.font;
        font.setPointSize(9);
        painter->setFont(font);
        painter->setPen(signing::kTealColor);
        QRect textRect = rect.adjusted(12, 0, 0, 0);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, index.data(Qt::DisplayRole).toString());
    } else {
        QFont font = option.font;
        font.setPointSize(9);
        painter->setFont(font);
        painter->setPen(option.palette.color(QPalette::Text));

        int rightMargin = isCustom ? 28 : 8;
        QRect textRect = rect.adjusted(12, 0, -rightMargin, 0);
        QFontMetrics fm(font);
        QString elided = fm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideMiddle, textRect.width());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);

        if (isCustom) {
            QRect btnRect = removeButtonRect(rect);
            QColor xColor = option.palette.color(QPalette::PlaceholderText);
            if (hovered)
                xColor = QColor(0xF4, 0x43, 0x36);
            QFont xFont = option.font;
            xFont.setPointSize(8);
            xFont.setBold(true);
            painter->setFont(xFont);
            painter->setPen(xColor);
            painter->drawText(btnRect, Qt::AlignCenter, QStringLiteral("\u2715"));
        }
    }

    painter->restore();
}

QSize TsaItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option)
    const QString type = index.data(Qt::UserRole).toString();
    return QSize(0, type == QStringLiteral("add") ? 26 : 28);
}

bool TsaItemDelegate::isAddItem(const QModelIndex& index) const
{
    return index.data(Qt::UserRole).toString() == QStringLiteral("add");
}

bool TsaItemDelegate::isCustomItem(const QModelIndex& index) const
{
    return index.data(Qt::UserRole).toString() == QStringLiteral("custom");
}
