// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "certificatetreeviewmodel.h"
#include "certificateinfoitem.h"
#include <QApplication>
#include <QFont>

CertificateTreeViewModel::CertificateTreeViewModel(QObject* parent)
    : QAbstractItemModel(parent)
    , rootItem(std::make_unique<CertificateInfoItem>())
{
}

CertificateTreeViewModel::~CertificateTreeViewModel() = default;

QVariant CertificateTreeViewModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::UserRole /*&& role != Qt::FontRole*/)
        return QVariant();

    auto* item = static_cast<CertificateInfoItem*>(index.internalPointer());

    if (role == Qt::UserRole)
        return item->data(1);

    // // Retrun bold, similar to windows cert viewer
    // if (role == Qt::FontRole && item->isCritical())
    // {
    //     QFont fnt = QApplication::font();
    //     fnt.setBold(true);
    //     return QVariant::fromValue(fnt);
    // }

    return item->data(index.column());
}

Qt::ItemFlags CertificateTreeViewModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return QAbstractItemModel::flags(index);
}

QVariant CertificateTreeViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return qtTrId("lc-cert-tree-field");
        case 1: return qtTrId("lc-cert-tree-value");
        }
    }
    return {};
}

QModelIndex CertificateTreeViewModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    CertificateInfoItem* parentItem;
    if (!parent.isValid())
        parentItem = rootItem.get();
    else
        parentItem = static_cast<CertificateInfoItem*>(parent.internalPointer());

    auto* childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return {};
}

QModelIndex CertificateTreeViewModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};

    auto* childItem = static_cast<CertificateInfoItem*>(index.internalPointer());
    auto* parentItem = childItem->parentItem();

    if (!parentItem || parentItem == rootItem.get())
        return {};

    return createIndex(parentItem->row(), 0, parentItem);
}

int CertificateTreeViewModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;

    CertificateInfoItem* parentItem;
    if (!parent.isValid())
        parentItem = rootItem.get();
    else
        parentItem = static_cast<CertificateInfoItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int CertificateTreeViewModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 2;
}
