// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "certificatetreeviewmodel.h"

typedef struct x509_st X509;
typedef struct x509_store_st X509_STORE;

class CertificateHierarchyModel : public CertificateTreeViewModel
{
    Q_OBJECT
public:
    explicit CertificateHierarchyModel(X509* cert, X509_STORE* store, QObject* parent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;

private:
    void buildChain(X509* cert, X509_STORE* store);
    QString translateVerificationResult(int error);

    int verificationError = 0;
};
