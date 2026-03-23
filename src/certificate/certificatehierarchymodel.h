// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include "certificatetreeviewmodel.h"

#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

class CertificateHierarchyModel : public CertificateTreeViewModel
{
    Q_OBJECT
public:
    explicit CertificateHierarchyModel(X509* cert, X509_STORE* store, QObject* parent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;

private:
    void buildChain(X509* cert, X509_STORE* store);
    QString translateVerificationResult(int error);

    int verificationError = X509_V_OK;
};
