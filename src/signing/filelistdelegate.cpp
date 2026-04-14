// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "filelistdelegate.h"

#include "signingcolors.h"

#include <QPainter>
#include <QStyleOptionViewItem>

FileListDelegate::FileListDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void FileListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QRect rect = option.rect;
    const bool selected = option.state & QStyle::State_Selected;

    // Selected background
    if (selected) {
        QColor selBg = signing::kTealColor;
        selBg.setAlpha(40);
        painter->fillRect(rect, selBg);
    }

    const QString fileName = index.data(FileNameRole).toString();
    const QString fileSize = index.data(FileSizeRole).toString();
    const QString fileType = index.data(FileTypeRole).toString();
    QString format = index.data(FormatDisplayRole).toString();
    if (format.isEmpty())
        format = index.data(FormatRole).toString();

    // Type badge color
    QColor badgeColor = signing::kTealColor;
    if (fileType == QStringLiteral("PDF"))
        badgeColor = QColor(0xE7, 0x4C, 0x3C);
    else if (fileType == QStringLiteral("XML"))
        badgeColor = QColor(0x4C, 0xAF, 0x50);
    else if (fileType == QStringLiteral("JSO"))
        badgeColor = QColor(0xFF, 0x98, 0x00);

    // Type badge: 8pt bold, at x+12, width 32
    QFont badgeFont = option.font;
    badgeFont.setPointSize(8);
    badgeFont.setBold(true);
    painter->setFont(badgeFont);
    painter->setPen(badgeColor);
    QRect badgeRect(rect.left() + 12, rect.top(), 32, rect.height());
    painter->drawText(badgeRect, Qt::AlignVCenter | Qt::AlignLeft, fileType);

    // Filename: 10pt, at x+48, elided middle, reserve 120px on right
    QFont nameFont = option.font;
    nameFont.setPointSize(10);
    painter->setFont(nameFont);
    painter->setPen(option.palette.color(QPalette::Text));
    const int rightReserved = 190;
    QRect nameRect(rect.left() + 48, rect.top(), rect.width() - 48 - rightReserved, rect.height());
    QFontMetrics nameFm(nameFont);
    QString elidedName = nameFm.elidedText(fileName, Qt::ElideMiddle, nameRect.width());
    painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft, elidedName);

    // Size: 8pt, palette(PlaceholderText), right-aligned in 60px rect
    QFont smallFont = option.font;
    smallFont.setPointSize(8);
    painter->setFont(smallFont);
    painter->setPen(option.palette.color(QPalette::PlaceholderText));
    QRect sizeRect(rect.right() - 180, rect.top(), 60, rect.height());
    painter->drawText(sizeRect, Qt::AlignVCenter | Qt::AlignRight, fileSize);

    // Format: teal, 8pt, right-aligned in 50px rect (rightmost)
    painter->setPen(signing::kTealColor);
    QRect formatRect(rect.right() - 120, rect.top(), 120, rect.height());
    painter->drawText(formatRect, Qt::AlignVCenter | Qt::AlignRight, format);

    painter->restore();
}

QSize FileListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(0, 28);
}
