// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The credential dialog: a launcher for the agent's PIN verbs.
///
/// It collects NOTHING. The code in force, the new one, a PUK — every secret
/// this flow needs is asked for by the agent's own prompter, in the agent's
/// own process, and none of it is ever seen, stored or carried here. What this
/// dialog owns is the DECISION (which verb, with which option), the rendering
/// of what the card reports about the credential and about each attempt, and
/// its own modal hygiene.

#pragma once

#include <LibreSCRS/AgentClient/CredentialTypes.h>
#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <LibreSCRS/AgentClient/SignOptions.h> // PinVerb, ManagePinOptions

#include <QDialog>
#include <QString>

class QCheckBox;
class QLabel;
class QPushButton;

namespace librecelik::agent {
class AgentGateway;
}

class ChangePinDlg : public QDialog
{
    Q_OBJECT
public:
    /// @p gateway may be nullptr in tests that only probe rendering; when set,
    /// the dialog OWNS its modal hygiene: it connects `gateway->cardRemoved(id)`
    /// and rejects itself when @p cardId matches — card pull AND agent loss
    /// (the gateway fans `cardRemoved` out on a presence drop) close it without
    /// any window glue.
    explicit ChangePinDlg(const LibreSCRS::AgentClient::CredentialRecord& record,
                          librecelik::agent::AgentGateway* gateway, const QString& cardId, QWidget* parent = nullptr);
    ~ChangePinDlg() override;

signals:
    /// The user chose a verb; secrets are collected by the agent prompter.
    void verbRequested(const QString& pinId, LibreSCRS::AgentClient::PinVerb verb,
                       const LibreSCRS::AgentClient::ManagePinOptions& options);

public slots:
    void onPhase(LibreSCRS::AgentClient::OperationPhase phase, double progress);
    void onPinResult(const LibreSCRS::AgentClient::PinResult& result);

private:
    /// Emit @p verb for this credential and put the dialog in the in-flight
    /// state: no second attempt may be launched while one is outstanding.
    void requestVerb(LibreSCRS::AgentClient::PinVerb verb);
    /// Which verb buttons the record currently permits. Re-applied after every
    /// attempt because an attempt can take a verb away (a blocked credential
    /// can no longer be changed) without a fresh listing having arrived yet.
    void applyVerbAvailability();

    LibreSCRS::AgentClient::CredentialRecord credential;
    QLabel* credentialLabel = nullptr;
    QLabel* stateLabel = nullptr;
    QLabel* guidanceLabel = nullptr;
    QLabel* hintLabel = nullptr;
    QLabel* phaseLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QCheckBox* activateKeyCheck = nullptr;
    QPushButton* changeButton = nullptr;
    QPushButton* unblockButton = nullptr;
    QPushButton* activateButton = nullptr;
    /// True between a verb emission and its result: the buttons are down and a
    /// second emission is refused.
    bool attemptInFlight = false;
};
