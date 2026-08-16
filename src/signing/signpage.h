// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "agent/signcontroller.h"

#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <LibreSCRS/AgentClient/SignOptions.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QList>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QWidget>

#include <optional>

class QEvent;
class QLabel;
class QListWidget;
class QProgressBar;

/// One selected document and the container it is signed into. The wizard
/// builds these on the selection page; the sign page hands them to the
/// controller unchanged, which is what partitions them into ceremonies.
struct FileSignInfo
{
    QString filePath;
    LibreSCRS::AgentClient::SignatureFormat format;
    LibreSCRS::AgentClient::Packaging packaging;
};

/// The wizard's last page: what is about to be signed, how many times the
/// holder will meet the agent's prompter for it, and one result row per
/// document once the run is over.
///
/// It collects NO secret. The PIN and every other credential are the agent
/// prompter's business, which is why this page has no input row at all and
/// why its progress line is the agent's own phase vocabulary rather than a
/// count of files this process is working through.
class SignPage : public QWidget
{
    Q_OBJECT
public:
    explicit SignPage(QWidget* parent = nullptr);
    ~SignPage() override;

    // Everything one signing run needs, bundled so the wizard can build it in
    // steps without a many-arg call and grow it later without breaking the
    // call site.
    struct Config
    {
        LibreSCRS::AgentClient::CertificateInfo certificate;
        /// The card the run belongs to. Carried with the run because a run IS
        /// of one card; the page never addresses that card itself — the
        /// controller it is handed is already the card's — so nothing here
        /// stores it.
        QString cardId;
        QList<FileSignInfo> fileInfos;
        /// UI level token ("B_B", "B_T", "B_LT", "B_LTA"); mapped for the wire
        /// by `librecelik::agent::levelFromUiToken`.
        QString level;
        QString outputFolder;
        /// Absent means an invisible signature and the request omits the map
        /// entirely; engaged carries the wire's six-key visualSignature map as
        /// `librecelik::agent::makeVisualSignatureMap` builds it.
        std::optional<QVariantMap> visual;
        QString tsaUrl;
    };

    void configure(Config cfg);
    /// NOT owned: the gateway mints one controller per card and parents it
    /// there. Held weakly, because a card that leaves takes its controller
    /// with it while this page may still be on screen.
    void setSignController(librecelik::agent::SignController* controller);
    [[nodiscard]] bool isSigningComplete() const;
    [[nodiscard]] bool isSigningInProgress() const;
    [[nodiscard]] bool hasFailures() const;

signals:
    void signingStarted();
    void signingFinished(int succeeded, int failed);

public slots:
    void startSigning();

protected:
    void changeEvent(QEvent* event) override;

private:
    void applyThemeColors();
    void addResultItem(const QString& icon, const QString& colorHex, const QString& message);
    /// How many times the holder will confirm at the agent prompter for the
    /// current selection: one per homogeneous run when the agent signs
    /// batches, one per file when it does not.
    [[nodiscard]] int ceremonyCount() const;
    void updateConsentHint();
    void onPhaseChanged(LibreSCRS::AgentClient::OperationPhase phase, double progress);
    void onRowFinished(int index, const librecelik::agent::SignRowResult& result);
    void onFinished(int succeeded, int failed);

    QWidget* summaryCard = nullptr;
    QLabel* certHeaderLabel = nullptr;
    QLabel* certValueLabel = nullptr;
    QLabel* filesHeaderLabel = nullptr;
    QLabel* filesValueLabel = nullptr;
    QLabel* levelHeaderLabel = nullptr;
    QLabel* levelValueLabel = nullptr;
    QLabel* consentHintLabel = nullptr;

    QWidget* progressWidget = nullptr;
    QLabel* progressLabel = nullptr;
    QProgressBar* progressBar = nullptr;

    QListWidget* resultsList = nullptr;

    /// Weak by construction: not owned, and destroyed with its card.
    QPointer<librecelik::agent::SignController> controller;
    LibreSCRS::AgentClient::CertificateInfo certificate;
    QList<FileSignInfo> fileInfos;
    QString sigLevel;
    QString outputDir;
    std::optional<QVariantMap> visual;
    QString tsaUrl;
    bool signingInProgress = false;
    bool signingComplete = false;
    int failedCount = 0;
};
