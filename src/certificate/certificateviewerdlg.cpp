// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificateviewerdlg.h"
#include "certificateviewerwidget.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>

#include <climits>
#include <filesystem>

namespace fs = std::filesystem;

CertificateViewerDlg::CertificateViewerDlg(const std::vector<plugin::CertificateData>& certs,
                                           const std::vector<std::string>& certPaths, QWidget* parent, int initialIndex)
    : QDialog(parent)
{
    setWindowTitle(qtTrId("lc-cert-dialog-title"));
    resize(600, 500);

    buildStore(certPaths);
    buildUI(certs);

    if (initialIndex > 0 && stack && certCombo) {
        certCombo->setCurrentIndex(initialIndex);
    } else if (initialIndex > 0 && stack) {
        stack->setCurrentIndex(initialIndex);
    }
}

// Destructor is defaulted in the header — RAII handles X509/X509_STORE cleanup.

void CertificateViewerDlg::buildStore(const std::vector<std::string>& certPaths)
{
    store.reset(X509_STORE_new());
    if (!store)
        return;

    X509_STORE_set_flags(store.get(), X509_V_FLAG_NO_CHECK_TIME);

    // Include system trust store for chain building
    X509_STORE_set_default_paths(store.get());

    auto loadCertFromData = [this](const QByteArray& data) {
        if (data.isEmpty() || data.size() > static_cast<qsizetype>(LONG_MAX))
            return;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data.constData());
        certutil::X509Ptr cert(d2i_X509(nullptr, &p, static_cast<long>(data.size())));
        if (!cert) {
            if (data.size() > static_cast<qsizetype>(INT_MAX))
                return;
            certutil::BioPtr bio(BIO_new_mem_buf(data.constData(), static_cast<int>(data.size())));
            if (bio)
                cert.reset(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
        }
        if (cert)
            X509_STORE_add_cert(store.get(), cert.get());
    };

    for (const auto& dirPath : certPaths) {
        if (dirPath.starts_with(":/")) {
            // Qt resource path — use QDirIterator for recursive enumeration
            QDirIterator it(QString::fromStdString(dirPath), {"*.cer", "*.crt", "*.pem"}, QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QFile file(it.next());
                if (file.open(QIODevice::ReadOnly))
                    loadCertFromData(file.readAll());
            }
        } else {
            // Filesystem path — load all cert files recursively. Skip
            // symlinks to avoid being tricked into reading files outside the
            // intended trust directory (and to skip permission-denied entries
            // gracefully).
            if (!fs::exists(dirPath))
                continue;
            std::error_code walkEc;
            for (auto it =
                     fs::recursive_directory_iterator(dirPath, fs::directory_options::skip_permission_denied, walkEc);
                 it != fs::recursive_directory_iterator(); it.increment(walkEc)) {
                if (walkEc) {
                    walkEc.clear();
                    continue;
                }
                const auto& entry = *it;
                if (entry.is_symlink())
                    continue;
                if (!entry.is_regular_file())
                    continue;
                auto ext = entry.path().extension().string();
                if (ext != ".cer" && ext != ".crt" && ext != ".pem")
                    continue;
                QFile file(QString::fromStdString(entry.path().string()));
                if (file.open(QIODevice::ReadOnly))
                    loadCertFromData(file.readAll());
            }
        }
    }
}

void CertificateViewerDlg::buildUI(const std::vector<plugin::CertificateData>& certs)
{
    auto* layout = new QVBoxLayout(this);

    // Parse DER bytes into X509Ptr
    for (const auto& cd : certs) {
        auto x509 = certutil::parseDer(cd.derBytes.data(), cd.derBytes.size());
        if (x509) {
            parsedCerts.push_back({std::move(x509), QString::fromStdString(cd.label)});
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
        auto* viewer = new CertificateViewerWidget(pc.x509.get(), store.get(), this);
        stack->addWidget(viewer);
    }
    layout->addWidget(stack);

    if (certCombo) {
        connect(certCombo, &QComboBox::currentIndexChanged, stack, &QStackedWidget::setCurrentIndex);
    }

    // Close button
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
