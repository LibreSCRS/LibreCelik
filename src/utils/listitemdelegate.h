// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QStyledItemDelegate>

class QAbstractItemView;

class ListItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ListItemDelegate(QAbstractItemView* view, QObject* parent = nullptr);
    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void addRequested();
    void removeRequested(int index);

protected:
    static QRect removeButtonRect(const QRect& itemRect);
    virtual bool isAddItem(const QModelIndex& index) const = 0;
    virtual bool isCustomItem(const QModelIndex& index) const = 0;
    QAbstractItemView* view;
};
