// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificateviewerdlg.h"
#include "certificateviewerwidget.h"

#include "agent/agentgateway.h"
#include "utils/buttonbox.h"
#include "utils/dialogs.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QPushButton>
#include <QVBoxLayout>

CertificateViewerDlg::CertificateViewerDlg(const LibreSCRS::AgentClient::CertificateInfo& cert,
                                           librecelik::agent::AgentGateway* gateway, QWidget* parent,
                                           const QString& readerId)
    : QDialog(parent), certificate(cert), gateway(gateway), readerId(readerId)
{
    setWindowTitle(
        qtTrId("lc-cert-dialog-title")); // i18n-audit: ignore D1, modal dialog re-opened fresh after language switch
    resize(600, 500);

    buildUI();

    if (!gateway)
        return;

    // One fetch on open serves both consumers of the raw bytes: the export
    // action saves them untouched, and an unparseable certificate's forensic
    // hex dump renders them. Until they arrive there is nothing to export, so
    // the action stays disabled rather than round-tripping on click.
    connect(gateway, &librecelik::agent::AgentGateway::certificateDerReady, this,
            &CertificateViewerDlg::onCertificateDerReady);
    gateway->fetchCertificateDer(readerId, certificate.id);
}

void CertificateViewerDlg::buildUI()
{
    auto* layout = new QVBoxLayout(this);

    viewer = new CertificateViewerWidget(certificate, this);
    layout->addWidget(viewer);

    auto* buttons = new librecelik::ButtonBox(QDialogButtonBox::Close, this);
    if (gateway) {
        // clang-format off
        exportButton = buttons->addButton(qtTrId("lc-cert-export-button"), QDialogButtonBox::ActionRole); // i18n-audit: ignore D1, modal dialog re-opened fresh after language switch
        // clang-format on
        exportButton->setEnabled(false);
        connect(exportButton, &QPushButton::clicked, this, &CertificateViewerDlg::exportCertificate);
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void CertificateViewerDlg::onCertificateDerReady(const QString& certId, const QByteArray& der)
{
    // The gateway signal is per-connection, not per-dialog: another viewer or
    // the token section may have asked for a different certificate.
    if (certId != certificate.id)
        return;

    certificateDer = der;
    if (viewer)
        viewer->setForensicDer(certificateDer);
    if (exportButton)
        exportButton->setEnabled(!certificateDer.isEmpty());
}

void CertificateViewerDlg::exportCertificate()
{
    if (certificateDer.isEmpty())
        return;

    // clang-format off
    const QString path = QFileDialog::getSaveFileName(this, qtTrId("lc-cert-export-save-title"), // i18n-audit: ignore D1, modal file dialog opened fresh per click
                                                      QStringLiteral("certificate.cer"), qtTrId("lc-cert-export-filter"));
    // clang-format on
    if (path.isEmpty())
        return;

    // The bytes go out exactly as the card produced them: no re-encoding, no
    // normalisation, no PEM wrapper. Whatever a forensic consumer does with
    // this file, it is looking at the card's own data.
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(certificateDer) != certificateDer.size() ||
        !file.flush()) {
        librecelik::dialogs::critical(this, qtTrId("lc-cert-dialog-title"), qtTrId("lc-cert-export-failed"));
    }
}
