// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/AgentClient/Types.h>

#include <QByteArray>
#include <QDialog>
#include <QString>

class CertificateViewerWidget;
class QPushButton;

namespace librecelik::agent {
class AgentGateway;
}

class CertificateViewerDlg : public QDialog
{
    Q_OBJECT
public:
    /// @brief Show one certificate as the agent described it.
    /// @param cert     The certificate record to render.
    /// @param gateway  Agent gateway used to fetch the raw certificate bytes.
    ///                 MAY BE NULL: the dialog then opens without the export
    ///                 action (the button is not created) and without the
    ///                 forensic hex dump for an unparseable certificate.
    ///                 Every other pane is fully functional — nothing else in
    ///                 this dialog needs the agent, because the certificate
    ///                 record already carries everything it renders.
    /// @param parent   Optional Qt parent.
    /// @param readerId Reader the certificate lives in. `certificateDer` is
    ///                 reader-addressed, so the export action needs it; it is
    ///                 unused when @p gateway is null.
    explicit CertificateViewerDlg(const LibreSCRS::AgentClient::CertificateInfo& cert,
                                  librecelik::agent::AgentGateway* gateway, QWidget* parent = nullptr,
                                  const QString& readerId = {});
    ~CertificateViewerDlg() override = default;

private:
    void buildUI();
    void onCertificateDerReady(const QString& certId, const QByteArray& der);
    void exportCertificate();

    LibreSCRS::AgentClient::CertificateInfo certificate;
    librecelik::agent::AgentGateway* gateway = nullptr;
    QString readerId;
    QByteArray certificateDer;
    CertificateViewerWidget* viewer = nullptr;
    QPushButton* exportButton = nullptr;
};
