// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QString>
#include <QVariant>
#include <memory>
#include <vector>

class CertificateInfoItem
{
public:
    explicit CertificateInfoItem(const QString& label = {}, const QString& value = {}, const bool critical = false,
                                 CertificateInfoItem* parent = nullptr);
    CertificateInfoItem(const QString& label, const QString& value, CertificateInfoItem* parent);

    ~CertificateInfoItem();

    void appendChild(std::unique_ptr<CertificateInfoItem> child);

    CertificateInfoItem* child(int row) const;
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    CertificateInfoItem* parentItem() const;

    bool isCritical() const;

private:
    QString label;
    QString value;
    bool critical;
    CertificateInfoItem* parent;
    std::vector<std::unique_ptr<CertificateInfoItem>> children;
};
