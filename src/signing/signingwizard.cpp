// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "signingwizard.h"

#include "certutils.h"
#include "fileselectionpage.h"
#include "signatureplacementpage.h"
#include "signpage.h"
#include "wizardheaderwidget.h"

#include <libresign/signing_service.h>

#include <QCloseEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

SigningWizard::SigningWizard(const plugin::CertificateData& cert, const std::string& readerName,
                             libresign::SigningService* signingService, QWidget* parent)
    : QDialog(parent), certificate(cert), readerName(readerName), signingService(signingService)
{
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
    signPage->setPrefetchCallback([this]() { return waitForPrefetch(); });

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

    // Make Next/Sign the default button so Enter triggers it
    nextBtn->setDefault(true);

    updateButtons();
}

SigningWizard::~SigningWizard()
{
    // The prefetchFuture lambda captures a raw pointer to signingService.
    // We must wait for it to finish to prevent use-after-free if the service
    // is destroyed shortly after the wizard. The wait is bounded by the
    // startup timeout (typically a few seconds).
    if (prefetchFuture.isValid() && !prefetchFuture.isFinished())
        prefetchFuture.waitForFinished();
}

void SigningWizard::setTrustConfig(const libresign::TrustConfig& config)
{
    signPage->setTrustConfig(config);

    // Start DSS service + trust config in background while user fills in wizard pages.
    // By the time user clicks Sign, the service is already warm.
    // Skip if the service is already running (e.g., from a previous wizard).
    if (signingService && !signingService->isAvailable()) {
        prefetchFuture = QtConcurrent::run([svc = signingService, trust = config]() -> bool {
            if (!trust.trustedLists.empty())
                return svc->configure(trust);
            return svc->isAvailable();
        });
    }
}

bool SigningWizard::waitForPrefetch()
{
    if (!prefetchFuture.isValid())
        return false;
    prefetchFuture.waitForFinished();
    if (prefetchFuture.isCanceled())
        return false;
    return prefetchFuture.result();
}

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
                libresign::VisualSignatureParams{},
                filePage->tsaUrl().toStdString(),
            });
            stack->setCurrentIndex(2);
        }
    } else if (current == 1) {
        // Placement → Sign
        placementPage->saveSettings();
        libresign::VisualSignatureParams visual;
        if (placementPage->isVisualSignatureEnabled()) {
            visual = placementPage->visualParams();
        }
        signPage->configure(SignPage::Config{
            certificate,
            readerName,
            buildFileInfoList(),
            filePage->signatureLevel(),
            filePage->outputFolder(),
            visual,
            filePage->tsaUrl().toStdString(),
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
