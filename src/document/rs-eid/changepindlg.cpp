// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "changepindlg.h"

#include "agent/agentgateway.h"
#include "agent/errortext.h"
#include "utils/buttonbox.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1StringView>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

#include <optional>

using LibreSCRS::AgentClient::CredentialKind;
using LibreSCRS::AgentClient::CredentialRecord;
using LibreSCRS::AgentClient::CredentialState;
using LibreSCRS::AgentClient::ManagePinOptions;
using LibreSCRS::AgentClient::OperationPhase;
using LibreSCRS::AgentClient::PinResult;
using LibreSCRS::AgentClient::PinVerb;
using LibreSCRS::AgentClient::RecoveryPath;

namespace {

/// The credential's display name. The agent authors the label; the opaque id
/// is the only honest fallback when it authored none (the token section's own
/// rule, kept identical so one credential reads the same in both places).
[[nodiscard]] QString displayName(const CredentialRecord& credential)
{
    return credential.label.isEmpty() ? credential.id : credential.label;
}

/// What the credential secures, in the holder's words. An unrecognised wire
/// kind renders nothing rather than a guess — the label above it already says
/// what the agent calls this credential.
[[nodiscard]] QString kindText(CredentialKind kind)
{
    switch (kind) {
    case CredentialKind::User:
        return qtTrId("lc-pin-kind-user");
    case CredentialKind::Sign:
        return qtTrId("lc-pin-kind-sign");
    case CredentialKind::Puk:
        return qtTrId("lc-pin-kind-puk");
    case CredentialKind::Can:
        return qtTrId("lc-pin-kind-can");
    case CredentialKind::Unknown:
    default:
        return {};
    }
}

/// Lifecycle badge, sharing the token section's copy so the row a holder
/// right-clicked and the dialog it opened cannot disagree.
[[nodiscard]] QString stateText(CredentialState state)
{
    switch (state) {
    case CredentialState::Transport:
        return qtTrId("lc-eid-pin-transport");
    case CredentialState::Blocked:
        return qtTrId("lc-eid-pin-blocked");
    case CredentialState::NeedsChange:
        return qtTrId("lc-eid-pin-needs-change");
    case CredentialState::Operational:
    case CredentialState::Unknown:
    default:
        return {};
    }
}

/// The retry counter, in whichever of the two shapes the card reported.
[[nodiscard]] QString retriesText(const CredentialRecord& credential)
{
    if (!credential.retriesLeft.has_value())
        return {};
    return credential.retriesMax.has_value()
               ? qtTrId("lc-eid-pin-tries-remaining-of").arg(*credential.retriesLeft).arg(*credential.retriesMax)
               : qtTrId("lc-eid-pin-tries-remaining").arg(*credential.retriesLeft);
}

/// Holder guidance for one guidance pair, on the error line's known-key rule:
/// a key THIS build names renders LC's own translated copy, an unknown key
/// falls through to the agent's authored fallback, and a key that arrives
/// without a usable fallback renders nothing rather than a guess made here.
///
/// The key vocabulary is the agent's and grows independently of this build,
/// which is exactly why the unknown arm exists and why it is the common one.
[[nodiscard]] QString guidanceText(const std::optional<QString>& key, const std::optional<QString>& fallback)
{
    const QString wireKey = key.value_or(QString());
    if (wireKey == QLatin1StringView("librescrs.pin.blocked.issuer"))
        return qtTrId("lc-pin-guidance-blocked-issuer");
    if (wireKey == QLatin1StringView("librescrs.pin.keyActivation.issuer"))
        return qtTrId("lc-pin-guidance-key-activation-issuer");

    const QString authored = fallback.value_or(QString());
    return authored.trimmed().isEmpty() ? QString() : authored;
}

/// The separator every multi-part line in this dialog joins on — the same
/// middle dot the token section's rows use.
[[nodiscard]] QString joinParts(const QStringList& parts)
{
    return parts.join(QStringLiteral(" · "));
}

} // namespace

ChangePinDlg::ChangePinDlg(const CredentialRecord& record, librecelik::agent::AgentGateway* gateway,
                           const QString& cardId, QWidget* parent)
    : QDialog(parent), credential(record)
{
    const QString name = displayName(credential);
    // clang-format off
    const auto initT = qtTrId("lc-changepin-initialize-title"); // i18n-audit: ignore D1, modal dialog re-opened fresh after language switch
    // clang-format on
    const auto changeT = qtTrId("lc-changepin-change-title");
    setWindowTitle(credential.state == CredentialState::Transport ? initT.arg(name) : changeT.arg(name));
    setMinimumWidth(400);

    auto* layout = new QVBoxLayout(this);

    credentialLabel = new QLabel(this);
    credentialLabel->setObjectName(QStringLiteral("credentialLabel"));
    credentialLabel->setWordWrap(true);
    const QString kind = kindText(credential.kind);
    credentialLabel->setText(kind.isEmpty() ? name : joinParts({name, kind}));
    layout->addWidget(credentialLabel);

    stateLabel = new QLabel(this);
    stateLabel->setObjectName(QStringLiteral("stateLabel"));
    stateLabel->setWordWrap(true);
    QStringList stateParts;
    if (const QString state = stateText(credential.state); !state.isEmpty())
        stateParts << state;
    if (const QString retries = retriesText(credential); !retries.isEmpty())
        stateParts << retries;
    stateLabel->setText(joinParts(stateParts));
    stateLabel->setVisible(!stateParts.isEmpty());
    layout->addWidget(stateLabel);

    guidanceLabel = new QLabel(this);
    guidanceLabel->setObjectName(QStringLiteral("guidanceLabel"));
    guidanceLabel->setWordWrap(true);
    QStringList guidance;
    if (credential.state == CredentialState::Blocked) {
        if (const QString blocked = guidanceText(credential.blockedGuidanceKey, credential.blockedGuidanceFallback);
            !blocked.isEmpty())
            guidance << blocked;
    }
    if (credential.keyActivationPending) {
        if (const QString keyActivation =
                guidanceText(credential.keyActivationGuidanceKey, credential.keyActivationGuidanceFallback);
            !keyActivation.isEmpty())
            guidance << keyActivation;
    }
    guidanceLabel->setText(guidance.join(QLatin1Char('\n')));
    guidanceLabel->setVisible(!guidance.isEmpty());
    layout->addWidget(guidanceLabel);

    // The whole point of this dialog: it says where the secrets will be asked
    // for, because they will not be asked for here.
    hintLabel = new QLabel(qtTrId("lc-pin-prompter-hint"), this);
    hintLabel->setObjectName(QStringLiteral("hintLabel"));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    activateKeyCheck = new QCheckBox(qtTrId("lc-pin-activate-key-check"), this);
    activateKeyCheck->setObjectName(QStringLiteral("activateKeyCheck"));
    // Offered only when a key is actually waiting: the option rides the SAME
    // mutation, so ticking it spares the holder a second prompt for a code
    // that has just been spent.
    activateKeyCheck->setVisible(credential.keyActivationPending);
    layout->addWidget(activateKeyCheck);

    phaseLabel = new QLabel(this);
    phaseLabel->setObjectName(QStringLiteral("phaseLabel"));
    phaseLabel->setWordWrap(true);
    phaseLabel->setVisible(false); // nothing has happened yet
    layout->addWidget(phaseLabel);

    statusLabel = new QLabel(this);
    statusLabel->setObjectName(QStringLiteral("statusLabel"));
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    auto* verbRow = new QHBoxLayout;
    changeButton = new QPushButton(qtTrId("lc-pin-verb-change"), this);
    changeButton->setObjectName(QStringLiteral("changeButton"));
    unblockButton = new QPushButton(qtTrId("lc-pin-verb-unblock"), this);
    unblockButton->setObjectName(QStringLiteral("unblockButton"));
    activateButton = new QPushButton(qtTrId("lc-pin-verb-activate"), this);
    activateButton->setObjectName(QStringLiteral("activateButton"));
    verbRow->addWidget(changeButton);
    verbRow->addWidget(unblockButton);
    verbRow->addWidget(activateButton);
    verbRow->addStretch();
    layout->addLayout(verbRow);

    auto* buttonBox = new librecelik::ButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(changeButton, &QPushButton::clicked, this, [this] { requestVerb(PinVerb::Change); });
    connect(unblockButton, &QPushButton::clicked, this, [this] { requestVerb(PinVerb::Unblock); });
    connect(activateButton, &QPushButton::clicked, this, [this] { requestVerb(PinVerb::ActivatePin); });

    applyVerbAvailability();

    // Modal hygiene lives HERE, not in the window that opened this dialog: a
    // card that leaves takes it along, and so does agent loss — the gateway
    // fans `cardRemoved` out over every card when presence drops.
    if (gateway != nullptr) {
        connect(gateway, &librecelik::agent::AgentGateway::cardRemoved, this,
                [this, cardId](const QString& removedCardId) {
                    if (removedCardId == cardId)
                        done(QDialog::Rejected);
                });
    }
}

ChangePinDlg::~ChangePinDlg() = default;

void ChangePinDlg::applyVerbAvailability()
{
    // Visible == the record permits this verb. A verb the card cannot perform
    // is absent from the dialog rather than present and dead: an offer that
    // always fails is worse than no offer.
    const bool blocked = credential.state == CredentialState::Blocked;
    changeButton->setVisible(credential.canChange);
    // Only the holder-recoverable path is offered. An issuer process is not
    // something this dialog can start, and the guidance line says so instead.
    unblockButton->setVisible(credential.unblockable && credential.recovery == RecoveryPath::HolderViaPuk);
    activateButton->setVisible(credential.activatable);

    changeButton->setEnabled(!attemptInFlight && !blocked);
    unblockButton->setEnabled(!attemptInFlight);
    activateButton->setEnabled(!attemptInFlight && !blocked);
    activateKeyCheck->setEnabled(!attemptInFlight);
}

void ChangePinDlg::requestVerb(PinVerb verb)
{
    if (attemptInFlight)
        return;
    attemptInFlight = true;
    statusLabel->setStyleSheet(QString());
    statusLabel->clear();
    applyVerbAvailability();

    ManagePinOptions options;
    // `activateKey` is legal only alongside ActivatePin; every other verb
    // sends no options at all.
    if (verb == PinVerb::ActivatePin)
        options.activateKey = activateKeyCheck->isChecked();

    emit verbRequested(credential.id, verb, options);
}

void ChangePinDlg::onPhase(OperationPhase phase, double progress)
{
    // The phase is the whole progress story a credential mutation has to tell:
    // its steps are not measurable in units, and the numeric fraction the
    // operation carries alongside them would put a moving bar on a wait whose
    // length is decided by a human at the prompter.
    Q_UNUSED(progress)
    phaseLabel->setText(librecelik::agent::phaseText(phase));
    phaseLabel->setVisible(true);
}

void ChangePinDlg::onPinResult(const PinResult& result)
{
    attemptInFlight = false;
    phaseLabel->setVisible(false);
    phaseLabel->clear();

    // The attempt is now history and the record moves with it: a blocked
    // result takes the change/activate verbs away before any fresh listing
    // arrives, and the counter the result carries replaces the one the dialog
    // opened with.
    if (result.blocked)
        credential.state = CredentialState::Blocked;
    if (result.retriesLeft.has_value())
        credential.retriesLeft = result.retriesLeft;

    QStringList parts;
    parts << librecelik::agent::outcomeText(result.outcome);
    if (result.retriesLeft.has_value())
        parts << qtTrId("lc-changepin-retries-remaining").arg(*result.retriesLeft);
    if (result.blocked)
        parts << qtTrId("lc-changepin-blocked");

    const bool ok = result.outcome == LibreSCRS::AgentClient::CredentialOutcome::Ok;
    statusLabel->setStyleSheet(ok ? QStringLiteral("color: green;") : QStringLiteral("color: red;"));
    statusLabel->setText(joinParts(parts));

    QStringList stateParts;
    if (const QString state = stateText(credential.state); !state.isEmpty())
        stateParts << state;
    if (const QString retries = retriesText(credential); !retries.isEmpty())
        stateParts << retries;
    stateLabel->setText(joinParts(stateParts));
    stateLabel->setVisible(!stateParts.isEmpty());

    applyVerbAvailability();
}
