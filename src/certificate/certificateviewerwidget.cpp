// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificateviewerwidget.h"
#include "ui_certificateviewerwidget.h"
#include "certificatepropertiesmodel.h"
#include "certificatehierarchymodel.h"
#include "opensslhelpers.h"

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <QLineEdit>

CertificateViewerWidget::CertificateViewerWidget(X509* cert, X509_STORE* store, QWidget* parent)
    : QWidget(parent), ui(new Ui::CertificateViewerWidget)
{
    ui->setupUi(this);

    if (!cert)
        return;

    populateGeneralTab(cert, store);

    // Details tab
    auto* propsModel = new CertificatePropertiesModel(cert, this);
    ui->detailsTreeView->setModel(propsModel);
    ui->detailsTreeView->expandAll();
    ui->detailsTreeView->resizeColumnToContents(0);

    connect(ui->detailsTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &CertificateViewerWidget::onDetailsSelectionChanged);

    // Certification Path tab
    if (store) {
        auto* hierarchyModel = new CertificateHierarchyModel(cert, store, this);
        ui->certPathTreeView->setModel(hierarchyModel);
        ui->certPathTreeView->expandAll();
        ui->certPathTreeView->resizeColumnToContents(0);

        connect(ui->certPathTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                &CertificateViewerWidget::onCertPathSelectionChanged);

        // Select the leaf node (deepest child) and populate summary
        QModelIndex idx = hierarchyModel->index(0, 0);
        while (hierarchyModel->rowCount(idx) > 0)
            idx = hierarchyModel->index(0, 0, idx);
        ui->certPathTreeView->setCurrentIndex(idx);
    }
}

CertificateViewerWidget::~CertificateViewerWidget()
{
    delete ui;
}

void CertificateViewerWidget::populateGeneralTab(X509* cert, X509_STORE* store)
{
    Q_UNUSED(store);

    X509_NAME* subject = X509_get_subject_name(cert);
    X509_NAME* issuer = X509_get_issuer_name(cert);

    auto setFieldText = [](QLineEdit* edit, const QString& text) {
        edit->setText(text);
        edit->setCursorPosition(0);
    };

    // Issued To
    setFieldText(ui->issuedToCNEdit, nameEntryValue(subject, NID_commonName));
    setFieldText(ui->issuedToOEdit, nameEntryValue(subject, NID_organizationName));
    setFieldText(ui->issuedToOUEdit, nameEntryValue(subject, NID_organizationalUnitName));

    ASN1_INTEGER* serial = X509_get_serialNumber(cert);
    BIGNUM* bn = ASN1_INTEGER_to_BN(serial, nullptr);
    if (bn) {
        char* hex = BN_bn2hex(bn);
        if (hex) {
            setFieldText(ui->issuedToSerialEdit, QString::fromLatin1(hex));
            OPENSSL_free(hex);
        }
        BN_free(bn);
    }

    // Issued By
    setFieldText(ui->issuedByCNEdit, nameEntryValue(issuer, NID_commonName));
    setFieldText(ui->issuedByOEdit, nameEntryValue(issuer, NID_organizationName));
    setFieldText(ui->issuedByOUEdit, nameEntryValue(issuer, NID_organizationalUnitName));

    // Validity
    setFieldText(ui->notBeforeEdit, certutil::asnTimeToString(X509_get0_notBefore(cert)));
    setFieldText(ui->notAfterEdit, certutil::asnTimeToString(X509_get0_notAfter(cert)));

    // Key Usage
    setFieldText(ui->keyUsageEdit, keyUsageString(cert));
}

QString CertificateViewerWidget::nameEntryValue(X509_NAME* name, int nid)
{
    if (!name)
        return {};

    int idx = X509_NAME_get_index_by_NID(name, nid, -1);
    if (idx < 0)
        return {};

    X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, idx);
    if (!entry)
        return {};

    ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
    if (!data)
        return {};

    unsigned char* utf8 = nullptr;
    int len = ASN1_STRING_to_UTF8(&utf8, data);
    if (len < 0)
        return {};

    QString result = QString::fromUtf8(reinterpret_cast<const char*>(utf8), len);
    OPENSSL_free(utf8);
    return result;
}

QString CertificateViewerWidget::keyUsageString(X509* cert)
{
    // Try Key Usage extension
    int idx = X509_get_ext_by_NID(cert, NID_key_usage, -1);
    if (idx < 0)
        idx = X509_get_ext_by_NID(cert, NID_ext_key_usage, -1);

    if (idx < 0)
        return {};

    X509_EXTENSION* ext = X509_get_ext(cert, idx);
    if (!ext)
        return {};

    certutil::BioPtr bio(BIO_new(BIO_s_mem()));
    if (!bio)
        return {};

    X509V3_EXT_print(bio.get(), ext, 0, 0);
    return certutil::bioToQString(bio.get());
}

void CertificateViewerWidget::onDetailsSelectionChanged(const QItemSelection& selected,
                                                        const QItemSelection& /*deselected*/)
{
    if (selected.indexes().isEmpty()) {
        ui->detailsValueEdit->clear();
        return;
    }

    QModelIndex index = selected.indexes().first();
    // Show the value from column 1 (Value column)
    QModelIndex valueIndex = index.sibling(index.row(), 1);
    QString value = valueIndex.data(Qt::DisplayRole).toString();
    ui->detailsValueEdit->setPlainText(value);
}

void CertificateViewerWidget::onCertPathSelectionChanged(const QItemSelection& selected,
                                                         const QItemSelection& /*deselected*/)
{
    if (selected.indexes().isEmpty()) {
        ui->certPathCNEdit->clear();
        ui->certPathStatusEdit->clear();
        return;
    }

    QModelIndex index = selected.indexes().first();
    QString cn = index.data(Qt::DisplayRole).toString();
    ui->certPathCNEdit->setText(cn);
    ui->certPathCNEdit->setCursorPosition(0);

    QModelIndex valueIndex = index.sibling(index.row(), 1);
    QString status = valueIndex.data(Qt::DisplayRole).toString();
    ui->certPathStatusEdit->setText(status.isEmpty() ? qtTrId("lc-cert-verify-valid") : status);
}
