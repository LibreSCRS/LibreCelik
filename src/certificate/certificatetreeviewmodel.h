// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QAbstractItemModel>
#include <memory>

class CertificateInfoItem;

class CertificateTreeViewModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit CertificateTreeViewModel(QObject* parent = nullptr);
    ~CertificateTreeViewModel() override;

    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;

protected:
    std::unique_ptr<CertificateInfoItem> rootItem;
};
