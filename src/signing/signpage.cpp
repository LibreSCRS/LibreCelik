// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "signpage.h"

#include "agent/errortext.h"
#include "agent/signrequest.h"
#include "resultdelegate.h"
#include "signingcolors.h"
#include "tsavalidation.h"
#include "utils/dialogs.h"

#include <QAction>
#include <QClipboard>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QProgressBar>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {

using LibreSCRS::AgentClient::OperationPhase;

// Result-row markers. Kept as literals next to the colours they are drawn in,
// exactly as the results list has always rendered them.
const QString kOkIcon = QStringLiteral("✔");
const QString kFailIcon = QStringLiteral("✘");

} // namespace

SignPage::SignPage(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    // --- Summary card ---
    summaryCard = new QWidget(this);
    auto* cardLayout = new QVBoxLayout(summaryCard);
    cardLayout->setContentsMargins(12, 10, 12, 10);

    auto* columns = new QHBoxLayout;

    auto makeColumn = [&](const QString& headerText, QLabel*& headerOut, QLabel*& valueOut) {
        auto* col = new QVBoxLayout;
        col->setSpacing(2);

        headerOut = new QLabel(headerText, summaryCard);
        QFont headerFont = headerOut->font();
        headerFont.setPointSize(7);
        headerFont.setCapitalization(QFont::AllUppercase);
        headerOut->setFont(headerFont);

        valueOut = new QLabel(summaryCard);
        QFont valueFont = valueOut->font();
        valueFont.setPointSize(9);
        valueOut->setFont(valueFont);

        col->addWidget(headerOut);
        col->addWidget(valueOut);
        return col;
    };

    columns->addLayout(makeColumn(qtTrId("lc-sign-summary-cert"), certHeaderLabel, certValueLabel));
    columns->addLayout(makeColumn(qtTrId("lc-sign-summary-files"), filesHeaderLabel, filesValueLabel));
    columns->addLayout(makeColumn(qtTrId("lc-sign-summary-level"), levelHeaderLabel, levelValueLabel));
    columns->addStretch();
    cardLayout->addLayout(columns);

    // The consent count belongs on the summary card because it is part of what
    // the holder is agreeing to: not just what gets signed, but how many times
    // they will be asked for their PIN to sign it.
    consentHintLabel = new QLabel(summaryCard);
    consentHintLabel->setObjectName(QStringLiteral("consentHint"));
    consentHintLabel->setWordWrap(true);
    QFont hintFont = consentHintLabel->font();
    hintFont.setPointSize(8);
    consentHintLabel->setFont(hintFont);
    consentHintLabel->setVisible(false);
    cardLayout->addWidget(consentHintLabel);

    summaryCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(summaryCard);
    layout->addSpacing(10);

    // --- Progress ---
    progressWidget = new QWidget(this);
    auto* progressLayout = new QVBoxLayout(progressWidget);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(4);

    progressLabel = new QLabel(progressWidget);
    progressLabel->setObjectName(QStringLiteral("progressLabel"));
    progressLayout->addWidget(progressLabel);

    progressBar = new QProgressBar(progressWidget);
    progressBar->setObjectName(QStringLiteral("progressBar"));
    progressBar->setFixedHeight(8);
    progressBar->setTextVisible(false);
    progressLayout->addWidget(progressBar);

    progressWidget->setVisible(false);
    layout->addWidget(progressWidget);

    layout->addSpacing(6);

    // --- Results list ---
    resultsList = new QListWidget(this);
    resultsList->setObjectName(QStringLiteral("resultsList"));
    resultsList->setItemDelegate(new ResultDelegate(resultsList));
    resultsList->setContextMenuPolicy(Qt::CustomContextMenu);
    resultsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    resultsList->setVisible(false);
    connect(resultsList, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = resultsList->itemAt(pos);
        if (!item)
            return;
        QMenu menu;
        auto* copyAction = menu.addAction(qtTrId("lc-sign-copy-error"));
        connect(copyAction, &QAction::triggered, this, [this]() {
            QStringList messages;
            for (auto* sel : resultsList->selectedItems())
                messages << sel->data(ResultDelegate::MessageRole).toString();
            if (!messages.isEmpty())
                QGuiApplication::clipboard()->setText(messages.join(QLatin1Char('\n')));
        });
        menu.exec(resultsList->mapToGlobal(pos));
    });
    layout->addWidget(resultsList, 1);

    layout->addStretch();

    applyThemeColors();
}

SignPage::~SignPage() = default;

void SignPage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        applyThemeColors();
    else if (event->type() == QEvent::LanguageChange) {
        certHeaderLabel->setText(qtTrId("lc-sign-summary-cert"));
        filesHeaderLabel->setText(qtTrId("lc-sign-summary-files"));
        levelHeaderLabel->setText(qtTrId("lc-sign-summary-level"));
        updateConsentHint();
    }
}

void SignPage::applyThemeColors()
{
    // Summary card: no stylesheet — let QPalette handle text colors naturally.
    // Use QPalette directly on labels for theme-safe colors.
    summaryCard->setStyleSheet(QString());

    auto setLabelColor = [](QLabel* label, const QColor& color) {
        QPalette pal = label->palette();
        pal.setColor(QPalette::WindowText, color);
        label->setPalette(pal);
    };

    const QColor placeholderColor = palette().color(QPalette::PlaceholderText);
    const QColor textColor = palette().color(QPalette::Text);
    setLabelColor(certHeaderLabel, placeholderColor);
    setLabelColor(filesHeaderLabel, placeholderColor);
    setLabelColor(levelHeaderLabel, placeholderColor);
    setLabelColor(certValueLabel, textColor);
    setLabelColor(filesValueLabel, textColor);
    setLabelColor(levelValueLabel, textColor);
    setLabelColor(consentHintLabel, placeholderColor);

    // Progress bar
    progressBar->setStyleSheet(QStringLiteral("QProgressBar { background: %1; border-radius: 4px; }"
                                              "QProgressBar::chunk { background: %2; border-radius: 4px; }")
                                   .arg(palette().color(QPalette::Midlight).name(), signing::kTealHex));

    // Progress label
    progressLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px;").arg(palette().color(QPalette::Text).name()));

    // Results list — no custom stylesheet, use native look
    resultsList->setStyleSheet(QString());
}

void SignPage::configure(Config cfg)
{
    certificate = std::move(cfg.certificate);
    fileInfos = std::move(cfg.fileInfos);
    sigLevel = std::move(cfg.level);
    outputDir = std::move(cfg.outputFolder);
    visual = std::move(cfg.visual);
    tsaUrl = std::move(cfg.tsaUrl);
    signingInProgress = false;
    signingComplete = false;
    failedCount = 0;

    // The subject names the certificate here exactly as it does in the token
    // list; the opaque id is the only honest fallback when the agent reported
    // no subject at all.
    certValueLabel->setText(certificate.subject.isEmpty() ? certificate.id : certificate.subject);
    filesValueLabel->setText(QString::number(fileInfos.count()));
    levelValueLabel->setText(QString(sigLevel).replace(QLatin1Char('_'), QLatin1Char('-')));
    updateConsentHint();

    progressWidget->setVisible(false);
    progressLabel->clear();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    resultsList->clear();
    resultsList->setVisible(false);
}

void SignPage::setSignController(librecelik::agent::SignController* newController)
{
    if (controller == newController)
        return;
    if (!controller.isNull())
        controller->disconnect(this);

    controller = newController;
    if (controller.isNull())
        return;

    connect(controller, &librecelik::agent::SignController::phaseChanged, this, &SignPage::onPhaseChanged);
    connect(controller, &librecelik::agent::SignController::rowFinished, this, &SignPage::onRowFinished);
    connect(controller, &librecelik::agent::SignController::finished, this, &SignPage::onFinished);

    // The controller answers whether the agent signs batches, which is the
    // other half of what the consent count is made of.
    updateConsentHint();
}

bool SignPage::isSigningComplete() const
{
    return signingComplete;
}

bool SignPage::isSigningInProgress() const
{
    return signingInProgress;
}

bool SignPage::hasFailures() const
{
    return failedCount > 0;
}

int SignPage::ceremonyCount() const
{
    if (fileInfos.isEmpty())
        return 0;

    // One ceremony per file is what a pre-batch agent costs, and it is also
    // the honest answer while no controller has been handed over: promising
    // fewer confirmations than the run will ask for is a consent-count lie.
    if (controller.isNull() || !controller->canBatch())
        return static_cast<int>(fileInfos.count());

    QList<librecelik::agent::SignRequestItem> items;
    items.reserve(fileInfos.count());
    for (const auto& info : fileInfos)
        items.append({info.filePath, info.format, info.packaging});
    // One run is one call is one consent — a run of a single document included.
    return static_cast<int>(librecelik::agent::partitionIntoRuns(items).count());
}

void SignPage::updateConsentHint()
{
    const int ceremonies = ceremonyCount();
    //% "You will confirm and enter your PIN in the system dialog %n times."
    consentHintLabel->setText(qtTrId("lc-sign-consent-hint", ceremonies));
    consentHintLabel->setVisible(ceremonies > 0);
}

void SignPage::startSigning()
{
    if (signingInProgress || signingComplete)
        return;
    if (controller.isNull() || fileInfos.isEmpty())
        return;

    // Pre-flight: the output folder must still be a writable existing
    // directory. The user could have selected one that has since been
    // deleted, made read-only, unmounted, or replaced by a regular file —
    // failing here gives a clear error instead of a string of per-row write
    // failures after every document has already been signed.
    {
        const QFileInfo outInfo(outputDir);
        if (outputDir.isEmpty() || !outInfo.exists() || !outInfo.isDir() || !outInfo.isWritable()) {
            librecelik::dialogs::warning(this, qtTrId("lc-sign-output-folder-error-title"),
                                         qtTrId("lc-sign-output-folder-error-message"));
            return;
        }
    }

    // Defensive: re-validate the TSA URL. FileSelectionPage already enforces
    // https+host for non-B_B levels, but configure() could be called directly
    // with anything — re-check so the agent is never handed an untrusted URL.
    if (sigLevel != QStringLiteral("B_B") && !tsaUrl.isEmpty() && !signing::isValidTsaUrl(tsaUrl)) {
        librecelik::dialogs::warning(this, qtTrId("lc-sign-tsa-invalid-title"), qtTrId("lc-sign-tsa-invalid-message"));
        return;
    }

    // An expired certificate at the baseline level: no revocation data is
    // embedded, so the signature can never be shown to have been made while
    // the certificate was valid. The agent honours the opt-in through the
    // documented "allowExpired" consent, which applies at that level only.
    bool allowExpired = false;
    if (sigLevel == QStringLiteral("B_B") && certificate.notAfter < QDateTime::currentDateTimeUtc()) {
        const auto answer = librecelik::dialogs::warning(this, qtTrId("lc-sign-expired-cert-title"),
                                                         qtTrId("lc-sign-expired-cert-message"),
                                                         QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        allowExpired = true;
    }

    LibreSCRS::AgentClient::SignOptions options;
    options.level = librecelik::agent::levelFromUiToken(sigLevel);
    if (visual)
        options.visualSignature = *visual;
    // Level decides whether the configured URL travels at all (tested helper):
    // the agent refuses tsaUrl on a baseline request, at submit, before any
    // card work.
    if (const QString effectiveTsa = signing::tsaUrlForLevel(sigLevel, tsaUrl); !effectiveTsa.isEmpty())
        options.tsaUrl = effectiveTsa;
    if (allowExpired)
        options.extra.insert(QStringLiteral("allowExpired"), true);

    // Format and packaging travel per item: the controller partitions the
    // selection into homogeneous runs and sets options.format/packaging from
    // the run it is about to dial.
    QList<librecelik::agent::SignRequestItem> items;
    items.reserve(fileInfos.count());
    for (const auto& info : fileInfos)
        items.append({info.filePath, info.format, info.packaging});

    signingInProgress = true;
    progressWidget->setVisible(true);
    progressBar->setRange(0, 0); // indeterminate until the first phase report
    progressLabel->setText(librecelik::agent::phaseText(OperationPhase::Created));
    resultsList->clear();
    resultsList->setVisible(true);

    emit signingStarted();
    controller->start(certificate.id, items, options, outputDir);
}

void SignPage::onPhaseChanged(OperationPhase phase, double progress)
{
    progressWidget->setVisible(true);
    progressLabel->setText(librecelik::agent::phaseText(phase));

    if (phase == OperationPhase::AwaitingConsent) {
        // The pace here is the holder's, not the agent's: a determinate bar
        // would animate a wait nobody is driving.
        progressBar->setRange(0, 0);
        return;
    }
    progressBar->setRange(0, 100);
    progressBar->setValue(static_cast<int>(std::clamp(progress, 0.0, 1.0) * 100.0));
}

void SignPage::onRowFinished(int index, const librecelik::agent::SignRowResult& result)
{
    // Rows appear in the order they finish, which is the order the runs were
    // dialled in — the selection-order index the controller carries is what
    // the run structure maps back to, not a position in this list.
    Q_UNUSED(index)

    const QString name = QFileInfo(result.filePath).fileName();
    if (result.ok) {
        addResultItem(kOkIcon, signing::kSuccessHex,
                      qtTrId("lc-sign-ok").arg(name, QFileInfo(result.outputPath).fileName()));
        return;
    }
    // Pass-through: the message is already the agent's own failure line,
    // localized where the wire vocabulary is mapped.
    addResultItem(kFailIcon, signing::kErrorHex, result.message);
}

void SignPage::onFinished(int succeeded, int failed)
{
    signingInProgress = false;
    signingComplete = true;
    failedCount = failed;

    progressBar->setRange(0, 1);
    progressBar->setValue(1);
    progressLabel->setText(qtTrId("lc-sign-complete").arg(succeeded).arg(failed));

    emit signingFinished(succeeded, failed);
}

void SignPage::addResultItem(const QString& icon, const QString& colorHex, const QString& message)
{
    auto* item = new QListWidgetItem(resultsList);
    item->setData(ResultDelegate::IconTextRole, icon);
    item->setData(ResultDelegate::IconColorRole, colorHex);
    item->setData(ResultDelegate::MessageRole, message);
}
