// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "listitemdelegate.h"

#include <QAbstractItemView>
#include <QMouseEvent>

ListItemDelegate::ListItemDelegate(QAbstractItemView* view, QObject* parent) : QStyledItemDelegate(parent), view(view)
{
    view->viewport()->installEventFilter(this);
}

bool ListItemDelegate::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() != QEvent::MouseButtonRelease)
        return QStyledItemDelegate::eventFilter(obj, event);

    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    QModelIndex index = view->indexAt(mouseEvent->pos());
    if (!index.isValid())
        return QStyledItemDelegate::eventFilter(obj, event);

    if (isAddItem(index)) {
        QMetaObject::invokeMethod(this, [this]() { emit addRequested(); }, Qt::QueuedConnection);
        return true;
    }

    if (isCustomItem(index)) {
        QRect itemRect = view->visualRect(index);
        QRect btnRect = removeButtonRect(itemRect);
        if (btnRect.contains(mouseEvent->pos())) {
            int row = index.row();
            QMetaObject::invokeMethod(this, [this, row]() { emit removeRequested(row); }, Qt::QueuedConnection);
            return true;
        }
    }

    return QStyledItemDelegate::eventFilter(obj, event);
}

QRect ListItemDelegate::removeButtonRect(const QRect& itemRect)
{
    int size = 20;
    int x = itemRect.right() - size - 4;
    int y = itemRect.top() + (itemRect.height() - size) / 2;
    return QRect(x, y, size, size);
}
