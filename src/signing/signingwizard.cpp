// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "signingwizard.h"

#include "agent/agentgateway.h"
#include "agent/signcontroller.h"
#include "fileselectionpage.h"
#include "signatureplacementpage.h"
#include "signpage.h"
#include "wizardheaderwidget.h"

#include <QByteArray>
#include <QCloseEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRectF>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVariantMap>

#include <optional>
#include <utility>

SigningWizard::SigningWizard(const LibreSCRS::AgentClient::CertificateInfo& cert, const QString& card,
                             librecelik::agent::AgentGateway* agentGateway, QWidget* parent)
    : QDialog(parent), certificate(cert), cardId(card), gateway(agentGateway)
{
    // One controller per card, minted and parented by the gateway. A card id
    // no longer in the roster answers null, and the wizard then offers a flow
    // it cannot run — so every use below is null-checked rather than assumed.
    controller = gateway != nullptr ? gateway->signController(cardId) : nullptr;

    setWindowTitle(qtTrId("lc-sign-wizard-title").arg(certificateCN()));
    setMinimumSize(600, 450);
    resize(700, 550);
    setModal(true);

    // Header
    headerWidget = new WizardHeaderWidget(this);
    headerWidget->setTitle(qtTrId("lc-sign-wizard-header-title"));
    headerWidget->setSubtitle(certificateCN());

    // Pages
    filePage = new FileSelectionPage(this);
    filePage->setObjectName(QStringLiteral("filePage"));
    placementPage = new SignaturePlacementPage(this);
    placementPage->setObjectName(QStringLiteral("placementPage"));
    signPage = new SignPage(this);
    signPage->setObjectName(QStringLiteral("signPage"));
    signPage->setSignController(controller);

    // What the connected agent can actually do decides what the pages offer:
    // no per-request timestamp authority means no TSA row, and no batch means
    // one confirmation per file — announced, never silently degraded.
    filePage->setCapabilities(controller != nullptr && controller->canTsaOverride(),
                              controller != nullptr && controller->canBatch());

    // Preview parity: the wrap, the font size and the glyph metrics all come
    // from whoever will stamp the page. A null gateway answers no layout, and
    // the preview then draws the placement box alone rather than inventing a
    // wrap of its own.
    placementPage->setLayoutProvider(
        [this](const QString& text, QRectF box) -> std::optional<LibreSCRS::AgentClient::LayoutResult> {
            if (gateway == nullptr)
                return std::nullopt;
            return gateway->layoutVisualSignature(text, box);
        });
    placementPage->setAppearanceFont(gateway != nullptr ? gateway->appearanceFontData() : QByteArray());

    stack = new QStackedWidget(this);
    stack->addWidget(filePage);      // index 0
    stack->addWidget(placementPage); // index 1
    stack->addWidget(signPage);      // index 2

    // Button bar
    backBtn = new QPushButton(qtTrId("lc-sign-btn-back"), this);
    nextBtn = new QPushButton(qtTrId("lc-sign-btn-next"), this);
    cancelBtn = new QPushButton(qtTrId("lc-sign-btn-cancel"), this);

    backBtn->setObjectName(QStringLiteral("backBtn"));
    nextBtn->setObjectName(QStringLiteral("nextBtn"));
    cancelBtn->setObjectName(QStringLiteral("cancelBtn"));

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addWidget(backBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(nextBtn);
    btnLayout->addWidget(cancelBtn);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 0, 8, 8);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(headerWidget);
    mainLayout->addWidget(stack, 1);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(backBtn, &QPushButton::clicked, this, &SigningWizard::goBack);
    connect(nextBtn, &QPushButton::clicked, this, &SigningWizard::goNext);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(filePage, &FileSelectionPage::validityChanged, this, [this](bool) { updateButtons(); });
    connect(signPage, &SignPage::signingStarted, this, [this]() {
        // Prevent closing wizard while signing is in progress
        cancelBtn->setEnabled(false);
        backBtn->setEnabled(false);
        nextBtn->setEnabled(false);
    });
    connect(signPage, &SignPage::signingFinished, this, [this](int, int failed) {
        cancelBtn->setEnabled(true);
        headerWidget->setAllComplete(failed == 0);
        updateButtons();
    });

    // Modal hygiene lives HERE, not in the window that opened us: a card that
    // leaves takes this dialog with it, and so does agent loss — the gateway
    // fans `cardRemoved` out over every card when presence drops.
    if (gateway != nullptr) {
        connect(gateway, &librecelik::agent::AgentGateway::cardRemoved, this, [this](const QString& removedCardId) {
            if (removedCardId == cardId)
                done(QDialog::Rejected);
        });
    }

    // Make Next/Sign the default button so Enter triggers it
    nextBtn->setDefault(true);

    updateButtons();
}

SigningWizard::~SigningWizard() = default;

void SigningWizard::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        setWindowTitle(qtTrId("lc-sign-wizard-title").arg(certificateCN()));
        headerWidget->setTitle(qtTrId("lc-sign-wizard-header-title"));
        updateButtons();
    }
    QDialog::changeEvent(event);
}

void SigningWizard::cancelIfSigning()
{
    if (signPage != nullptr && signPage->isSigningInProgress() && controller != nullptr)
        controller->cancel();
}

void SigningWizard::closeEvent(QCloseEvent* event)
{
    // Asking to close a run in flight IS asking to abandon it, so the request
    // goes to the controller first. The dialog itself stays up until the
    // controller reports a terminal — never blocking on it, and never
    // vanishing with the result list before it has one to show.
    cancelIfSigning();
    if (signPage && !signPage->isSigningComplete() && stack->currentIndex() == 2 && !cancelBtn->isEnabled()) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void SigningWizard::reject()
{
    cancelIfSigning();
    if (signPage && !signPage->isSigningComplete() && stack->currentIndex() == 2 && !cancelBtn->isEnabled())
        return;
    QDialog::reject();
}

void SigningWizard::goNext()
{
    int current = stack->currentIndex();

    if (current == 0) {
        // A visible signature is offered only when the agent can both stamp
        // one and lay it out for the preview: LC never offers a placement it
        // cannot show honestly.
        const bool visualOffered = controller != nullptr && controller->canVisualSign();
        if (visualOffered && filePage->hasPAdESFiles()) {
            // Show placement page for PAdES files
            placementPage->loadPdf(filePage->firstPAdESFile(), certificateCN(), certificate.issuer);
            placementShown = true;
            headerWidget->setPlacementShown(true);
            stack->setCurrentIndex(1);
        } else {
            // No PDF files, or no visible signature to place — go straight to
            // sign with an invisible one.
            placementShown = false;
            headerWidget->setPlacementShown(false);
            signPage->configure(SignPage::Config{
                certificate,
                cardId,
                buildFileInfoList(),
                filePage->signatureLevel(),
                filePage->outputFolder(),
                std::nullopt,
                filePage->tsaUrl(),
            });
            stack->setCurrentIndex(2);
        }
    } else if (current == 1) {
        // Placement → Sign
        placementPage->saveSettings();
        std::optional<QVariantMap> visual;
        if (placementPage->isVisualSignatureEnabled())
            visual = placementPage->visualSignatureMap();
        signPage->configure(SignPage::Config{
            certificate,
            cardId,
            buildFileInfoList(),
            filePage->signatureLevel(),
            filePage->outputFolder(),
            std::move(visual),
            filePage->tsaUrl(),
        });
        stack->setCurrentIndex(2);
    } else if (current == 2) {
        // On sign page, Next becomes "Sign"
        signPage->startSigning();
    }

    updateButtons();
}

void SigningWizard::goBack()
{
    int current = stack->currentIndex();

    if (current == 2) {
        stack->setCurrentIndex(placementShown ? 1 : 0);
    } else if (current == 1) {
        stack->setCurrentIndex(0);
    }

    updateButtons();
}

void SigningWizard::updateButtons()
{
    int current = stack->currentIndex();
    headerWidget->setCurrentStep(current);
    bool signingDone = signPage->isSigningComplete();

    if (current == 0) {
        backBtn->setEnabled(false);
        nextBtn->setEnabled(!filePage->selectedFiles().isEmpty());
        nextBtn->setText(qtTrId("lc-sign-btn-next"));
        cancelBtn->setText(qtTrId("lc-sign-btn-cancel"));
    } else if (current == 1) {
        backBtn->setEnabled(true);
        nextBtn->setEnabled(true);
        nextBtn->setText(qtTrId("lc-sign-btn-next"));
        cancelBtn->setText(qtTrId("lc-sign-btn-cancel"));
    } else if (current == 2) {
        if (signingDone) {
            backBtn->setEnabled(signPage->hasFailures());
            nextBtn->setEnabled(false);
            cancelBtn->setText(qtTrId("lc-sign-btn-done"));
        } else if (signPage->isSigningInProgress()) {
            backBtn->setEnabled(false);
            nextBtn->setEnabled(false);
            cancelBtn->setEnabled(false);
        } else {
            backBtn->setEnabled(true);
            // Nothing is collected on this page any more, so the only thing
            // Sign waits on is having something to sign and someone to sign
            // it with.
            nextBtn->setEnabled(controller != nullptr && !filePage->selectedFiles().isEmpty());
            nextBtn->setText(qtTrId("lc-sign-btn-sign"));
            cancelBtn->setText(qtTrId("lc-sign-btn-cancel"));
        }
    }
}

QList<FileSignInfo> SigningWizard::buildFileInfoList() const
{
    QList<FileSignInfo> infos;
    const QStringList selectedFiles = filePage->selectedFiles();
    infos.reserve(selectedFiles.count());
    for (int i = 0; i < selectedFiles.count(); ++i)
        infos.append({selectedFiles.at(i), filePage->formatForFile(i), filePage->packagingForFile(i)});
    return infos;
}

QString SigningWizard::certificateCN() const
{
    // Honest without parsing anything: both transports feed the certificate's
    // common name into `subject`, so this is a CN and not a full DN to
    // truncate. The opaque id is the only fallback when none was reported.
    return certificate.subject.isEmpty() ? certificate.id : certificate.subject;
}
