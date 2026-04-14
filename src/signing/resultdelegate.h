// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QStyledItemDelegate>

class ResultDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    enum Role {
        IconColorRole = Qt::UserRole + 100,
        IconTextRole = Qt::UserRole + 101,
        MessageRole = Qt::UserRole + 102
    };

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
