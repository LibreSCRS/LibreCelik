// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QStyledItemDelegate>

class FileListDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    enum Role { FileNameRole = Qt::UserRole, FileSizeRole, FileTypeRole, FormatRole, PackagingRole, FormatDisplayRole };

    explicit FileListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
