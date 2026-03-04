// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "certificateviewerdlg.h"
#include "certificateviewerwidget.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <openssl/bio.h>
#include <openssl/pem.h>

#include <QDir>
#include <QFile>

CertificateViewerDlg::CertificateViewerDlg(const eidcard::CertificateList& certs,
                                             const std::string& certFolderPath,
                                             QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(qtTrId("lc-cert-dialog-title"));
    resize(600, 500);

    buildStore(certFolderPath);
    buildUI(certs);
}

CertificateViewerDlg::~CertificateViewerDlg()
{
    for (auto& pc : parsedCerts) {
        if (pc.x509)
            X509_free(pc.x509);
    }
    if (store)
        X509_STORE_free(store);
}

void CertificateViewerDlg::buildStore(const std::string& certFolderPath)
{
    store = X509_STORE_new();
    if (!store)
        return;

    X509_STORE_set_flags(store, X509_V_FLAG_NO_CHECK_TIME);

    QDir dir(QString::fromStdString(certFolderPath));
    if (!dir.exists())
        return;

    for (const QString& name : dir.entryList({"*.cer", "*.crt", "*.pem"}, QDir::Files)) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QByteArray data = file.readAll();
        if (data.isEmpty())
            continue;

        const uint8_t* p = reinterpret_cast<const uint8_t*>(data.constData());
        X509* cert = d2i_X509(nullptr, &p, static_cast<long>(data.size()));
        if (!cert) {
            BIO* bio = BIO_new_mem_buf(data.constData(), static_cast<int>(data.size()));
            if (bio) {
                cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
                BIO_free(bio);
            }
        }
        if (cert) {
            X509_STORE_add_cert(store, cert);
            X509_free(cert);
        }
    }
}

void CertificateViewerDlg::buildUI(const eidcard::CertificateList& certs)
{
    auto* layout = new QVBoxLayout(this);

    // Parse DER bytes into X509*
    for (const auto& cd : certs) {
        const uint8_t* p = cd.derBytes.data();
        X509* x509 = d2i_X509(nullptr, &p, static_cast<long>(cd.derBytes.size()));
        if (x509) {
            parsedCerts.push_back({ x509, QString::fromStdString(cd.label) });
        }
    }

    if (parsedCerts.empty()) {
        layout->addWidget(new QLabel(qtTrId("lc-cert-no-available")));
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
        return;
    }

    // Certificate selector
    if (parsedCerts.size() > 1) {
        certCombo = new QComboBox(this);
        for (const auto& pc : parsedCerts)
            certCombo->addItem(pc.label);
        layout->addWidget(certCombo);
    }

    // Stacked widget with one viewer per cert
    stack = new QStackedWidget(this);
    for (const auto& pc : parsedCerts) {
        auto* viewer = new CertificateViewerWidget(pc.x509, store, this);
        stack->addWidget(viewer);
    }
    layout->addWidget(stack);

    if (certCombo) {
        connect(certCombo, &QComboBox::currentIndexChanged,
                stack, &QStackedWidget::setCurrentIndex);
    }

    // Close button
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
