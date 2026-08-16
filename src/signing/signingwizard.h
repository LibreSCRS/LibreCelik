// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/AgentClient/Types.h>

#include <QDialog>
#include <QList>
#include <QString>

struct FileSignInfo;
class FileSelectionPage;
class SignaturePlacementPage;
class SignPage;
class WizardHeaderWidget;
class QStackedWidget;
class QPushButton;

namespace librecelik::agent {
class AgentGateway;
class SignController;
} // namespace librecelik::agent

/// The three-step signing flow for one certificate on one card: pick the
/// documents, place the visible signature, sign.
///
/// It owns no card, no session and no secret. The gateway hands it the card's
/// sign controller, the agent collects every credential at its own prompter,
/// and the wizard's whole job on the last page is to say what is about to
/// happen and to report what did.
class SigningWizard : public QDialog
{
    Q_OBJECT
public:
    /// @param cert     The certificate to sign with, as the agent described it.
    /// @param cardId   The card that certificate lives on; the controller and
    ///                 the removal guard are both addressed by it.
    /// @param gateway  Agent gateway — NOT owned; it outlives this modal
    ///                 dialog, which is the window's own lifetime assumption.
    SigningWizard(const LibreSCRS::AgentClient::CertificateInfo& cert, const QString& cardId,
                  librecelik::agent::AgentGateway* gateway, QWidget* parent = nullptr);
    ~SigningWizard() override;

protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    void goNext();
    void goBack();
    void updateButtons();
    /// Ask the controller to stop when a run is in flight. Fire-and-forget by
    /// the client's contract: the terminal still arrives through the
    /// controller's own `finished`, and nothing here waits for it.
    void cancelIfSigning();
    [[nodiscard]] QString certificateCN() const;
    [[nodiscard]] QList<FileSignInfo> buildFileInfoList() const;

    WizardHeaderWidget* headerWidget = nullptr;
    QStackedWidget* stack = nullptr;
    FileSelectionPage* filePage = nullptr;
    SignaturePlacementPage* placementPage = nullptr;
    SignPage* signPage = nullptr;
    QPushButton* backBtn = nullptr;
    QPushButton* nextBtn = nullptr;
    QPushButton* cancelBtn = nullptr;
    LibreSCRS::AgentClient::CertificateInfo certificate;
    QString cardId;
    /// Neither of these is owned: the gateway belongs to the window and the
    /// controller to the gateway, which parents one per card.
    librecelik::agent::AgentGateway* gateway = nullptr;
    librecelik::agent::SignController* controller = nullptr;
    bool placementShown = false;
};
