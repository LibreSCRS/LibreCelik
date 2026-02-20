// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "certificatepropertiesmodel.h"
#include "certificateinfoitem.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>
#include <QRegularExpression>
#include <QString>

CertificatePropertiesModel::CertificatePropertiesModel(X509* cert, QObject* parent)
    : CertificateTreeViewModel(parent)
{
    if (cert)
        buildTree(cert);
}

void CertificatePropertiesModel::buildTree(X509* cert)
{
    addVersion(rootItem.get(), cert);
    addSerialNumber(rootItem.get(), cert);
    addSignatureAlgorithm(rootItem.get(), cert);
    addIssuer(rootItem.get(), cert);
    addValidity(rootItem.get(), cert);
    addSubject(rootItem.get(), cert);
    addPublicKeyInfo(rootItem.get(), cert);
    addExtensions(rootItem.get(), cert);
}

void CertificatePropertiesModel::addVersion(CertificateInfoItem* parent, X509* cert)
{
    long ver = X509_get_version(cert) + 1;
    parent->appendChild(std::make_unique<CertificateInfoItem>(
        tr("Version"), QString("V%1").arg(ver), parent));
}

void CertificatePropertiesModel::addSerialNumber(CertificateInfoItem* parent, X509* cert)
{
    ASN1_INTEGER* serial = X509_get_serialNumber(cert);
    BIGNUM* bn = ASN1_INTEGER_to_BN(serial, nullptr);
    if (bn) {
        char* hex = BN_bn2hex(bn);
        if (hex) {
            parent->appendChild(std::make_unique<CertificateInfoItem>(
                tr("Serial Number"), QString::fromLatin1(hex), parent));
            OPENSSL_free(hex);
        }
        BN_free(bn);
    }
}

void CertificatePropertiesModel::addSignatureAlgorithm(CertificateInfoItem* parent, X509* cert)
{
    const X509_ALGOR* alg = X509_get0_tbs_sigalg(cert);
    if (alg) {
        const ASN1_OBJECT* obj = nullptr;
        X509_ALGOR_get0(&obj, nullptr, nullptr, alg);
        if (obj) {
            char buf[128];
            OBJ_obj2txt(buf, sizeof(buf), obj, 0);
            parent->appendChild(std::make_unique<CertificateInfoItem>(
                tr("Signature Algorithm"), QString::fromLatin1(buf), parent));
        }
    }
}

void CertificatePropertiesModel::addIssuer(CertificateInfoItem* parent, X509* cert)
{
    X509_NAME* issuer = X509_get_issuer_name(cert);
    parent->appendChild(std::make_unique<CertificateInfoItem>(
        tr("Issuer"), nameToString(issuer), parent));
}

void CertificatePropertiesModel::addValidity(CertificateInfoItem* parent, X509* cert)
{
    auto validityItem = std::make_unique<CertificateInfoItem>(
        tr("Validity"), QString(), parent);
    auto* validityPtr = validityItem.get();

    validityPtr->appendChild(std::make_unique<CertificateInfoItem>(
        tr("Not Before"), asnTimeToString(X509_get0_notBefore(cert)), validityPtr));
    validityPtr->appendChild(std::make_unique<CertificateInfoItem>(
        tr("Not After"), asnTimeToString(X509_get0_notAfter(cert)), validityPtr));

    parent->appendChild(std::move(validityItem));
}

void CertificatePropertiesModel::addSubject(CertificateInfoItem* parent, X509* cert)
{
    X509_NAME* subject = X509_get_subject_name(cert);
    parent->appendChild(std::make_unique<CertificateInfoItem>(
        tr("Subject"), nameToString(subject), parent));
}

void CertificatePropertiesModel::addPublicKeyInfo(CertificateInfoItem* parent, X509* cert)
{
    EVP_PKEY* pkey = X509_get0_pubkey(cert);
    if (!pkey)
        return;

    auto pkItem = std::make_unique<CertificateInfoItem>(
        tr("Public Key Info"), QString(), parent);
    auto* pkPtr = pkItem.get();

    int keyType = EVP_PKEY_base_id(pkey);
    const char* typeName = OBJ_nid2ln(keyType);
    pkPtr->appendChild(std::make_unique<CertificateInfoItem>(
        tr("Algorithm"), typeName ? QString::fromLatin1(typeName) : tr("Unknown"), pkPtr));

    int bits = EVP_PKEY_bits(pkey);
    pkPtr->appendChild(std::make_unique<CertificateInfoItem>(
        tr("Key Size"), QString("%1 bits").arg(bits), pkPtr));

    parent->appendChild(std::move(pkItem));
}

void CertificatePropertiesModel::addExtensions(CertificateInfoItem* parent, X509* cert)
{
    int extCount = X509_get_ext_count(cert);
    if (extCount <= 0)
        return;

    auto extItem = std::make_unique<CertificateInfoItem>(
        tr("Extensions"), QString("(%1)").arg(extCount), parent);
    auto* extPtr = extItem.get();

    for (int i = 0; i < extCount; i++) {
        X509_EXTENSION* ext = X509_get_ext(cert, i);
        if (!ext)
            continue;

        ASN1_OBJECT* obj = X509_EXTENSION_get_object(ext);
        char oidBuf[128];
        OBJ_obj2txt(oidBuf, sizeof(oidBuf), obj, 0);

        BIO* bio = BIO_new(BIO_s_mem());
        if (bio) {
            X509V3_EXT_print(bio, ext, X509V3_EXT_DUMP_UNKNOWN, 0);
            BUF_MEM* bptr = nullptr;
            BIO_get_mem_ptr(bio, &bptr);
            QString value;
            if (bptr && bptr->length > 0)
                value = QString::fromUtf8(bptr->data, static_cast<int>(bptr->length));
            BIO_free(bio);

            if (value.contains(":")) {
                value.replace(QRegularExpression("([0-9A-Fa-f]{2}):"), "\\1");
            }

            bool critical = X509_EXTENSION_get_critical(ext);
            QString label = QString::fromLatin1(oidBuf);
            if (critical)
                label += tr(" (Critical)");

            extPtr->appendChild(std::make_unique<CertificateInfoItem>(
                label, value, critical, extPtr));
        }
    }

    parent->appendChild(std::move(extItem));
}

QString CertificatePropertiesModel::nameToString(X509_NAME* name)
{
    if (!name)
        return {};

    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        return {};

    X509_NAME_print_ex(bio, name, 0,
                       (ASN1_STRFLGS_UTF8_CONVERT | XN_FLAG_MULTILINE | XN_FLAG_DN_REV) & ~ASN1_STRFLGS_ESC_MSB);
    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(bio, &bptr);
    QString result;
    if (bptr && bptr->length > 0)
        result = QString::fromUtf8(bptr->data, static_cast<int>(bptr->length));
    BIO_free(bio);
    return result;
}

QString CertificatePropertiesModel::asnTimeToString(const ASN1_TIME* time)
{
    if (!time)
        return {};

    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        return {};

    ASN1_TIME_print(bio, time);
    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(bio, &bptr);
    QString result;
    if (bptr && bptr->length > 0)
        result = QString::fromLatin1(bptr->data, static_cast<int>(bptr->length));
    BIO_free(bio);
    return result;
}

QString CertificatePropertiesModel::toHexString(const unsigned char* data, int len)
{
    QString result;
    for (int i = 0; i < len; i++) {
        if (i > 0)
            result += ':';
        result += QString("%1").arg(data[i], 2, 16, QChar('0')).toUpper();
    }
    return result;
}
