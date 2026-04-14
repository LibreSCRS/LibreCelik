// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/listitemdelegate.h"

class TlItemDelegate : public ListItemDelegate
{
    Q_OBJECT
public:
    enum Role {
        TypeRole = Qt::UserRole + 1, // "default" | "custom" | "add"
        IsLotlRole,                  // bool
        EagerRole,                   // bool
    };

    explicit TlItemDelegate(QAbstractItemView* view, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

protected:
    bool isAddItem(const QModelIndex& index) const override;
    bool isCustomItem(const QModelIndex& index) const override;
};
