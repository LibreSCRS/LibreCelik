// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "tlitemdelegate.h"

#include "signing/signingcolors.h"

#include <QPainter>

TlItemDelegate::TlItemDelegate(QAbstractItemView* view, QObject* parent) : ListItemDelegate(view, parent) {}

void TlItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QRect rect = option.rect;
    const QString type = index.data(TypeRole).toString();
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
        painter->drawText(rect.adjusted(12, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
                          index.data(Qt::DisplayRole).toString());
    } else {
        // URL text
        QFont font = option.font;
        font.setPointSize(9);
        painter->setFont(font);
        painter->setPen(option.palette.color(QPalette::Text));

        int rightMargin = isCustom ? 28 : 8;
        int badgeSpace = 120;
        QRect textRect = rect.adjusted(12, 0, -(rightMargin + badgeSpace), 0);
        QFontMetrics fm(font);
        QString url = index.data(Qt::DisplayRole).toString();
        QString elided = fm.elidedText(url, Qt::ElideMiddle, textRect.width());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);

        // Badges
        QFont badgeFont = option.font;
        badgeFont.setPointSize(7);
        badgeFont.setBold(true);
        painter->setFont(badgeFont);

        int badgeX = rect.right() - rightMargin - badgeSpace;
        int badgeY = rect.top() + (rect.height() - 16) / 2;

        // TL/LOTL badge
        bool isLotl = index.data(IsLotlRole).toBool();
        QString typeLabel = isLotl ? QStringLiteral("LOTL") : QStringLiteral("TL");
        QColor typeBg = isLotl ? QColor(0x7B, 0x1F, 0xA2) : signing::kTealColor;
        QRect typeBadge(badgeX, badgeY, 36, 16);
        painter->setBrush(typeBg);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(typeBadge, 3, 3);
        painter->setPen(Qt::white);
        painter->drawText(typeBadge, Qt::AlignCenter, typeLabel);

        // Eager/Lazy badge
        bool eager = index.data(EagerRole).toBool();
        QString loadLabel = eager ? QStringLiteral("Eager") : QStringLiteral("Lazy");
        QColor loadBg = eager ? QColor(0x2E, 0x7D, 0x32) : QColor(0x9E, 0x9E, 0x9E);
        QRect loadBadge(badgeX + 40, badgeY, 42, 16);
        painter->setBrush(loadBg);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(loadBadge, 3, 3);
        painter->setPen(Qt::white);
        painter->drawText(loadBadge, Qt::AlignCenter, loadLabel);

        // Remove button for custom items
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
            painter->setBrush(Qt::NoBrush);
            painter->drawText(btnRect, Qt::AlignCenter, QStringLiteral("\u2715"));
        }
    }

    painter->restore();
}

QSize TlItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option)
    const QString type = index.data(TypeRole).toString();
    return QSize(0, type == QStringLiteral("add") ? 26 : 32);
}

bool TlItemDelegate::isAddItem(const QModelIndex& index) const
{
    return index.data(TypeRole).toString() == QStringLiteral("add");
}

bool TlItemDelegate::isCustomItem(const QModelIndex& index) const
{
    return index.data(TypeRole).toString() == QStringLiteral("custom");
}
