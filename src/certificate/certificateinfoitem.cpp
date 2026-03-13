// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "certificateinfoitem.h"

CertificateInfoItem::CertificateInfoItem(const QString& label, const QString& value, const bool critical,
                                         CertificateInfoItem* parent)
    : label(label), value(value), critical(critical), parent(parent)
{}

CertificateInfoItem::CertificateInfoItem(const QString& label, const QString& value, CertificateInfoItem* parent)
    : label(label), value(value), critical(false), parent(parent)
{}

CertificateInfoItem::~CertificateInfoItem() = default;

void CertificateInfoItem::appendChild(std::unique_ptr<CertificateInfoItem> child)
{
    children.push_back(std::move(child));
}

CertificateInfoItem* CertificateInfoItem::child(int row) const
{
    if (row < 0 || row >= static_cast<int>(children.size()))
        return nullptr;
    return children[row].get();
}

int CertificateInfoItem::childCount() const
{
    return static_cast<int>(children.size());
}

int CertificateInfoItem::columnCount() const
{
    return 2;
}

QVariant CertificateInfoItem::data(int column) const
{
    switch (column) {
    case 0:
        return label;
    case 1:
        return value;
    default:
        return {};
    }
}

int CertificateInfoItem::row() const
{
    if (!parent)
        return 0;

    for (int i = 0; i < static_cast<int>(parent->children.size()); ++i) {
        if (parent->children[i].get() == this)
            return i;
    }
    return 0;
}

CertificateInfoItem* CertificateInfoItem::parentItem() const
{
    return parent;
}

bool CertificateInfoItem::isCritical() const
{
    return critical;
}
