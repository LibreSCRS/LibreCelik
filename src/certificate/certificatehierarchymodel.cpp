// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "certificatehierarchymodel.h"
#include "certificateinfoitem.h"
#include "utils/libreceliklog.h"

#include <openssl/bio.h>
#include <openssl/x509.h>

#include <QIcon>

CertificateHierarchyModel::CertificateHierarchyModel(X509* cert, X509_STORE* store,
                                                       QObject* parent)
    : CertificateTreeViewModel(parent)
{
    if (cert && store)
        buildChain(cert, store);
}

QVariant CertificateHierarchyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (role == Qt::DecorationRole && index.column() == 0) {
        auto* item = static_cast<CertificateInfoItem*>(index.internalPointer());
        // Leaf item gets a status icon
        if (item->childCount() == 0) {
            if (verificationError == X509_V_OK)
                return QIcon(":/images/green_checked.png");
            else
                return QIcon(":/images/red_exclamation.png");
        }
    }

    return CertificateTreeViewModel::data(index, role);
}

void CertificateHierarchyModel::buildChain(X509* cert, X509_STORE* store)
{
    X509_STORE_CTX* ctx = X509_STORE_CTX_new();
    if (!ctx)
        return;

    if (X509_STORE_CTX_init(ctx, store, cert, nullptr) != 1) {
        X509_STORE_CTX_free(ctx);
        return;
    }

    X509_verify_cert(ctx);
    verificationError = X509_STORE_CTX_get_error(ctx);

    STACK_OF(X509)* chain = X509_STORE_CTX_get1_chain(ctx);
    X509_STORE_CTX_free(ctx);

    if (!chain)
        return;

    // Build tree from root (last in chain) to leaf (first)
    int count = sk_X509_num(chain);
    CertificateInfoItem* current = rootItem.get();

    for (int i = count - 1; i >= 0; i--) {
        X509* c = sk_X509_value(chain, i);
        X509_NAME* subject = X509_get_subject_name(c);

        QString label;
        int cnIdx = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
        if (cnIdx >= 0) {
            X509_NAME_ENTRY* entry = X509_NAME_get_entry(subject, cnIdx);
            if (entry) {
                ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
                if (data) {
                    unsigned char* utf8 = nullptr;
                    int len = ASN1_STRING_to_UTF8(&utf8, data);
                    if (len > 0) {
                        label = QString::fromUtf8(reinterpret_cast<const char*>(utf8), len);
                        OPENSSL_free(utf8);
                    }
                }
            }
        }
        if (label.isEmpty())
            label = tr("Unknown");

        // For the leaf, add verification status
        QString value;
        if (i == 0) {
            value = translateVerificationResult(verificationError);
            X509_STORE_CTX_set_error(ctx, verificationError);
        }

        auto item = std::make_unique<CertificateInfoItem>(label, value, current);
        auto* ptr = item.get();
        current->appendChild(std::move(item));
        current = ptr;
    }

    sk_X509_pop_free(chain, X509_free);
}

QString CertificateHierarchyModel::translateVerificationResult(int error)
{
    switch (error) {
    case X509_V_OK:
        return tr("Valid");
    case X509_V_ERR_UNSPECIFIED:
        return tr("Unspecified error");
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
        return tr("Unable to get issuer certificate");
    case X509_V_ERR_UNABLE_TO_GET_CRL:
        return tr("Unable to get CRL");
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
        return tr("Unable to decrypt certificate signature");
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
        return tr("Unable to decode issuer public key");
    case X509_V_ERR_CERT_NOT_YET_VALID:
        return tr("Certificate not yet valid");
    case X509_V_ERR_CERT_HAS_EXPIRED:
        return tr("Certificate has expired");
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
        return tr("Certificate signature failure");
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
        return tr("Error in cert not before field");
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
        return tr("Error in cert not after field");
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
        return tr("Self-signed certificate");
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        return tr("Self-signed certificate in chain");
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        return tr("Unable to get local issuer certificate");
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        return tr("Unable to verify leaf signature");
    case X509_V_ERR_CERT_REVOKED:
        return tr("Certificate revoked");
    default:
    {
        verificationError = X509_V_OK;
        qCDebug(libreCelikCertificates) << "Info cert error: " << QString::fromUtf8(X509_verify_cert_error_string(error));
        return tr("Valid");
    }
    }
}
