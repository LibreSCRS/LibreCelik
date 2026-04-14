// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "certificatetreeviewmodel.h"

typedef struct x509_st X509;
typedef struct X509_name_st X509_NAME;

class CertificatePropertiesModel : public CertificateTreeViewModel
{
    Q_OBJECT
public:
    explicit CertificatePropertiesModel(X509* cert, QObject* parent = nullptr);

private:
    void buildTree(X509* cert);
    void addVersion(CertificateInfoItem* parent, X509* cert);
    void addSerialNumber(CertificateInfoItem* parent, X509* cert);
    void addSignatureAlgorithm(CertificateInfoItem* parent, X509* cert);
    void addIssuer(CertificateInfoItem* parent, X509* cert);
    void addValidity(CertificateInfoItem* parent, X509* cert);
    void addSubject(CertificateInfoItem* parent, X509* cert);
    void addPublicKeyInfo(CertificateInfoItem* parent, X509* cert);
    void addExtensions(CertificateInfoItem* parent, X509* cert);

    static QString nameToString(X509_NAME* name);
};
