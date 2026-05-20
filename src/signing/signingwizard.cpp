// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "signingwizard.h"

#include "certutils.h"
#include "fileselectionpage.h"
#include "signatureplacementpage.h"
#include "signpage.h"
#include "wizardheaderwidget.h"

#include <LibreSCRS/Signing/SigningService.h>
#include <LibreSCRS/Signing/VisualSignatureLayout.h>
#include <LibreSCRS/Signing/VisualSignatureParams.h>

#include <QByteArray>
#include <QCloseEvent>
#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QtGlobal>
#include <QVBoxLayout>

#include <mutex>

namespace {

// Register the bundled Liberation Sans TTF (sourced from LibreMiddleware) so
// the LC preview surface (PdfPreviewWidget) renders appearance text using the
// exact same font as the embedded PDF subset. Idempotent: std::once_flag
// guarantees a single registration per process; failure is non-fatal — the
// preview falls back to system sans-serif (per spec §8.5).
//
// Singleton-policy note (per feedback_singleton_patterns.md): this is NOT a
// classical singleton or Meyers idiom. The function-local std::once_flag is a
// synchronization token (an initialization barrier), not stateful storage.
// The actual state lives inside QFontDatabase, which is already a Qt-owned
// global. Q_GLOBAL_STATIC would be appropriate if LC owned the singleton's
// storage; here we only need a one-shot init guard for an external global.
void ensureAppearanceFontRegistered() noexcept
{
    static std::once_flag flag;
    std::call_once(flag, []() noexcept {
        const auto bytes = LibreSCRS::Signing::embeddedAppearanceFontData();
        if (bytes.empty()) {
            qWarning("LibreSCRS: embedded Liberation Sans data is empty; "
                     "preview will fall back to system sans-serif");
            return;
        }
        const QByteArray ba(reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()));
        if (QFontDatabase::addApplicationFontFromData(ba) < 0)
            qWarning("LibreSCRS: failed to register Liberation Sans for preview; "
                     "falling back to system sans-serif");
    });
}

} // namespace

SigningWizard::SigningWizard(const LibreSCRS::Plugin::CertificateData& cert, const std::string& readerName,
                             std::shared_ptr<LibreSCRS::Signing::SigningService> service,
                             std::shared_ptr<LibreSCRS::Plugin::CardPlugin> plugin,
                             std::shared_ptr<LibreSCRS::SmartCard::CardSession> cardSession, QWidget* parent)
    : QDialog(parent), certificate(cert), readerName(readerName), signingService(std::move(service)),
      session(std::move(cardSession)), cardPlugin(std::move(plugin))
{
    Q_ASSERT(cardPlugin != nullptr);
    Q_ASSERT(session != nullptr);

    // Register the bundled Liberation Sans TTF on first wizard construction
    // so PdfPreviewWidget renders with the same font as the embedded PDF
    // subset (preview-vs-PDF parity). The function-local static guarantees
    // a single registration per process; subsequent wizards skip the call.
    ensureAppearanceFontRegistered();

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
    placementPage = new SignaturePlacementPage(this);
    signPage = new SignPage(this);
    signPage->setSigningService(signingService);

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
    connect(signPage, &SignPage::pinReady, this, [this](bool ready) {
        if (stack->currentIndex() == 2)
            nextBtn->setEnabled(ready);
    });
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

    // Auto-close-on-removal is wired up by the caller (LibreCelik) so the
    // wizard stays decoupled from any QObject-typed removal source: the
    // unit-test fixture does not link the LibreCelik MOC and would
    // otherwise see an undefined-reference at link time. See
    // `librecelik.cpp` next to the `wizard.exec()` call site for the
    // matching connect on `LibreCelik::cardRemoved` — it filters by the
    // same `readerName` we hold here.

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

void SigningWizard::closeEvent(QCloseEvent* event)
{
    if (signPage && !signPage->isSigningComplete() && stack->currentIndex() == 2 && !cancelBtn->isEnabled()) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void SigningWizard::reject()
{
    if (signPage && !signPage->isSigningComplete() && stack->currentIndex() == 2 && !cancelBtn->isEnabled())
        return;
    QDialog::reject();
}

void SigningWizard::goNext()
{
    int current = stack->currentIndex();

    if (current == 0) {
        if (filePage->hasPAdESFiles()) {
            // Show placement page for PAdES files
            auto names = signing::certNames(certificate.derBytes);
            QString cn = names.subjectCN.isEmpty() ? QString::fromStdString(certificate.label) : names.subjectCN;
            placementPage->loadPdf(filePage->firstPAdESFile(), cn, names.issuerCN);
            placementShown = true;
            headerWidget->setPlacementShown(true);
            stack->setCurrentIndex(1);
        } else {
            // No PDF files — skip placement, go directly to sign
            placementShown = false;
            headerWidget->setPlacementShown(false);
            signPage->configure(SignPage::Config{
                certificate,
                readerName,
                buildFileInfoList(),
                filePage->signatureLevel(),
                filePage->outputFolder(),
                std::nullopt,
                filePage->tsaUrl().toStdString(),
                cardPlugin,
                session,
                session && session->hasLiveSecureChannel(),
            });
            stack->setCurrentIndex(2);
        }
    } else if (current == 1) {
        // Placement → Sign
        placementPage->saveSettings();
        std::optional<LibreSCRS::Signing::VisualSignatureParams> visual;
        if (placementPage->isVisualSignatureEnabled())
            visual = placementPage->visualParams();
        signPage->configure(SignPage::Config{
            certificate,
            readerName,
            buildFileInfoList(),
            filePage->signatureLevel(),
            filePage->outputFolder(),
            std::move(visual),
            filePage->tsaUrl().toStdString(),
            cardPlugin,
            session,
            session && session->hasLiveSecureChannel(),
        });
        stack->setCurrentIndex(2);
    } else if (current == 2) {
        // On sign page, Next becomes "Sign"
        signPage->startSigning();
    }

    // Auto-focus PIN field when entering sign page
    if (stack->currentIndex() == 2 && !signPage->isSigningComplete())
        signPage->focusPin();

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
            nextBtn->setEnabled(signPage->hasPinInput());
            nextBtn->setText(qtTrId("lc-sign-btn-sign"));
            cancelBtn->setText(qtTrId("lc-sign-btn-cancel"));
        }
    }
}

QList<FileSignInfo> SigningWizard::buildFileInfoList() const
{
    QList<FileSignInfo> infos;
    auto selectedFiles = filePage->selectedFiles();
    for (int i = 0; i < selectedFiles.count(); ++i) {
        infos.append({selectedFiles.at(i), filePage->formatForFile(i), filePage->packagingForFile(i)});
    }
    return infos;
}

QString SigningWizard::certificateCN() const
{
    QString cn = signing::subjectCN(certificate.derBytes);
    return cn.isEmpty() ? QString::fromStdString(certificate.label) : cn;
}
