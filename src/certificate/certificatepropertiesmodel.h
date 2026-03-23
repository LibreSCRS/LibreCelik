// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CERTIFICATEPROPERTIESMODEL_H
#define CERTIFICATEPROPERTIESMODEL_H

#include "certificatetreeviewmodel.h"

#include <openssl/x509.h>

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
    static QString asnTimeToString(const ASN1_TIME* time);
};

#endif // CERTIFICATEPROPERTIESMODEL_H
