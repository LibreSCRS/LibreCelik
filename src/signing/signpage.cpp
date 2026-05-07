// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "signpage.h"

#include "certutils.h"
#include "pkcs11utils.h"
#include "resultdelegate.h"
#include "signingcolors.h"
#include "utils/iconutils.h"

#include <LibreSCRS/Auth/AuthRequirement.h>
#include <LibreSCRS/Auth/CredentialProvider.h>
#include <LibreSCRS/Auth/CredentialResult.h>
#include <LibreSCRS/Plugin/CardPlugin.h>
#include <LibreSCRS/Signing/SigningRequest.h>
#include <LibreSCRS/Signing/SigningResult.h>
#include <LibreSCRS/Signing/SigningService.h>
#include <LibreSCRS/Signing/TsaProvider.h>
#include <LibreSCRS/SmartCard/CardSession.h>
#include <LibreSCRS/Secure/String.h>

#include <filesystem>

#include <QAction>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QProgressBar>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

SignPage::SignPage(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    // --- Summary card ---
    summaryCard = new QWidget(this);
    auto* cardLayout = new QHBoxLayout(summaryCard);
    cardLayout->setContentsMargins(12, 10, 12, 10);

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

    cardLayout->addLayout(makeColumn(qtTrId("lc-sign-summary-cert"), certHeaderLabel, certValueLabel));
    cardLayout->addLayout(makeColumn(qtTrId("lc-sign-summary-files"), filesHeaderLabel, filesValueLabel));
    cardLayout->addLayout(makeColumn(qtTrId("lc-sign-summary-level"), levelHeaderLabel, levelValueLabel));
    cardLayout->addStretch();

    summaryCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(summaryCard);
    layout->addSpacing(10);

    // --- CAN entry (for contactless eMRTD cards) ---
    canRow = new QWidget(this);
    auto* canRowLayout = new QHBoxLayout(canRow);
    canRowLayout->setContentsMargins(0, 0, 0, 0);
    //% "CAN:"
    canLabel = new QLabel(qtTrId("lc-sign-can-label"), canRow);
    canRowLayout->addWidget(canLabel);
    canEdit = new QLineEdit(canRow);
    canEdit->setMaximumWidth(160);
    // CAN is a 6-digit numeric code on Serbian eID / eMRTD cards, with
    // longer variants (up to ~10 digits) defined in some EU profiles. We
    // gate input to digits only via QRegularExpressionValidator instead
    // of QIntValidator, which overflows for maxLength > 9.
    canEdit->setMaxLength(10);
    canEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d*$")), canEdit));
    canRowLayout->addWidget(canEdit);
    canRowLayout->addStretch();
    canRow->setVisible(false); // shown only for CL readers
    layout->addWidget(canRow);

    // --- PIN entry ---
    pinRow = new QWidget(this);
    auto* pinRowLayout = new QHBoxLayout(pinRow);
    pinRowLayout->setContentsMargins(0, 0, 0, 0);
    //% "PIN:"
    pinLabel = new QLabel(qtTrId("lc-sign-pin-label"), pinRow);
    pinRowLayout->addWidget(pinLabel);

    pinEdit = new QLineEdit(pinRow);
    pinEdit->setEchoMode(QLineEdit::Password);
    pinEdit->setMaximumWidth(160);
    pinEdit->setMaxLength(256);
    pinEdit->setAttribute(Qt::WA_InputMethodEnabled, false);
    iconutils::addToggleVisibilityAction(this, pinEdit);
    pinRowLayout->addWidget(pinEdit);
    pinRowLayout->addStretch();
    layout->addWidget(pinRow);

    layout->addSpacing(10);

    // --- Progress ---
    progressWidget = new QWidget(this);
    auto* progressLayout = new QVBoxLayout(progressWidget);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(4);

    progressLabel = new QLabel(progressWidget);
    progressLayout->addWidget(progressLabel);

    progressBar = new QProgressBar(progressWidget);
    progressBar->setFixedHeight(8);
    progressBar->setTextVisible(false);
    progressLayout->addWidget(progressBar);

    progressWidget->setVisible(false);
    layout->addWidget(progressWidget);

    layout->addSpacing(6);

    // --- Results list ---
    resultsList = new QListWidget(this);
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

    // PIN validity drives the wizard's Sign button
    auto checkReady = [this]() {
        bool pinOk = !pinEdit->text().isEmpty() && !signingComplete;
        bool canOk = !canRow->isVisible() || !canEdit->text().isEmpty();
        emit pinReady(pinOk && canOk);
    };
    connect(pinEdit, &QLineEdit::textChanged, this, checkReady);
    connect(canEdit, &QLineEdit::textChanged, this, checkReady);

    applyThemeColors();
}

SignPage::~SignPage()
{
    // Thread safety: the worker thread posts UI updates via QMetaObject::invokeMethod
    // with a QPointer<SignPage> guard (see startSigning). The guard ensures that if
    // this object is destroyed before a queued invocation runs, the call is silently
    // dropped. We still disconnect signals and wait here for orderly shutdown.
    //
    // The wait is BOUNDED so a smart card or PKCS#11 module that hangs cannot
    // block application shutdown indefinitely. PKCS#11 sign operations are not
    // cancellable mid-call (SCardCancel is a no-op for SCardTransmit on Linux),
    // so if the module truly hangs we accept leaking the worker thread (the
    // OS reaps it on process exit) rather than freezing the entire app. The
    // PIN material is held in a LibreSCRS::Secure::String captured by the
    // lambda, so the leaked thread does NOT leave PIN bytes around
    // indefinitely either — the String destructor still cleanses when the
    // thread eventually returns.
    //
    // The worker captures shared_ptr<CardPlugin> and shared_ptr<CardSession>
    // — their lifetime is provably bounded by the captured shared_ptrs, so
    // if the 30-second wait expires and the thread keeps running the module
    // is still accessing live objects (no UAF). SigningService::sign itself
    // also retains the shared_ptrs for the duration of the call.
    if (workerThread && workerThread->isRunning()) {
        workerThread->disconnect(this);
        constexpr int kShutdownWaitMs = 30000; // 30s — PC/SC transmit timeout plus PKCS#11 sign headroom
        if (!workerThread->wait(kShutdownWaitMs)) {
            qWarning("SignPage: worker thread did not finish within %dms; "
                     "leaking thread to avoid blocking shutdown. The worker "
                     "holds shared_ptr<CardPlugin> and shared_ptr<CardSession> "
                     "so the underlying objects remain live until the thread "
                     "eventually completes.",
                     kShutdownWaitMs);
            // Intentionally NOT calling terminate() — that would leave the
            // PKCS#11 module in undefined state. Detach instead: the existing
            // QThread::finished → deleteLater connection still self-cleans
            // when the operation eventually returns.
            workerThread = nullptr;
        }
    }
}

void SignPage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        applyThemeColors();
    else if (event->type() == QEvent::LanguageChange) {
        certHeaderLabel->setText(qtTrId("lc-sign-summary-cert"));
        filesHeaderLabel->setText(qtTrId("lc-sign-summary-files"));
        levelHeaderLabel->setText(qtTrId("lc-sign-summary-level"));
        pinLabel->setText(qtTrId("lc-sign-pin-label"));
        canLabel->setText(qtTrId("lc-sign-can-label"));
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

    // PIN focus border
    pinEdit->setStyleSheet(QStringLiteral("QLineEdit:focus { border: 1px solid %1; }").arg(signing::kTealHex));

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
    Q_ASSERT(cfg.cardPlugin != nullptr);
    Q_ASSERT(cfg.session != nullptr);

    certificate = cfg.certificate;
    readerName = cfg.readerName;
    fileInfos = cfg.fileInfos;
    sigLevel = cfg.level;
    outputDir = cfg.outputFolder;
    visualParams = std::move(cfg.visual);
    tsaUrl = cfg.tsaUrl;
    cardPlugin = std::move(cfg.cardPlugin);
    session = std::move(cfg.session);
    signingInProgress = false;
    signingComplete = false;
    failedCount = 0;

    // Heuristic: reader name contains CL/Contactless. May false-positive for non-PACE readers.
    const QString readerQ = QString::fromStdString(cfg.readerName);
    const bool isCL = readerQ.contains(QStringLiteral("CL"), Qt::CaseInsensitive) ||
                      readerQ.contains(QStringLiteral("Contactless"), Qt::CaseInsensitive);
    canRow->setVisible(isCL);

    certValueLabel->setText(QString::fromStdString(cfg.certificate.label));
    filesValueLabel->setText(QString::number(cfg.fileInfos.count()));
    levelValueLabel->setText(QString(cfg.level).replace(QLatin1Char('_'), QLatin1Char('-')));

    pinRow->setVisible(true);
    pinEdit->setEnabled(true);
    progressWidget->setVisible(false);
    progressLabel->clear();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    resultsList->clear();
    resultsList->setVisible(false);

    bool canOk = !canRow->isVisible() || !canEdit->text().isEmpty();
    emit pinReady(!pinEdit->text().isEmpty() && canOk);
}

void SignPage::setSigningService(std::shared_ptr<LibreSCRS::Signing::SigningService> svc)
{
    signingService = std::move(svc);
}

bool SignPage::hasPinInput() const
{
    bool canOk = !canRow->isVisible() || !canEdit->text().isEmpty();
    return !pinEdit->text().isEmpty() && canOk;
}

void SignPage::focusPin()
{
    // If CAN field is visible and empty, focus it first; otherwise focus PIN
    if (canRow->isVisible() && canEdit->text().isEmpty())
        canEdit->setFocus();
    else
        pinEdit->setFocus();
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

namespace {
std::string refreshTimestampIn(std::string text)
{
    static const QRegularExpression dateRe(QStringLiteral("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}"));
    QString q = QString::fromStdString(text);
    q.replace(dateRe, QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    return q.toStdString();
}

LibreSCRS::Signing::VisualSignatureParams
cloneWithRefreshedTimestamp(const LibreSCRS::Signing::VisualSignatureParams& src)
{
    LibreSCRS::Signing::VisualSignatureParams::Builder b;
    b.pageIndex(src.pageIndex())
        .rect(LibreSCRS::Signing::Rect{src.x(), src.y(), src.width(), src.height()})
        .textTemplate(refreshTimestampIn(src.textTemplate()));
    return std::move(b).build();
}
} // namespace

void SignPage::startSigning()
{
    if (workerThread && workerThread->isRunning())
        return;

    if (!signingService || fileInfos.isEmpty() || !cardPlugin || !session)
        return;

    // Pre-flight: validate the output folder is a writable existing
    // directory before we kick off any worker. The user could have selected
    // a folder that has since been deleted, made read-only, unmounted, or
    // replaced by a regular file. Failing here gives a clear error instead
    // of a string of per-file write failures from the signing loop.
    {
        QFileInfo outInfo(outputDir);
        if (outputDir.isEmpty() || !outInfo.exists() || !outInfo.isDir() || !outInfo.isWritable()) {
            QMessageBox::warning(this, qtTrId("lc-sign-output-folder-error-title"),
                                 qtTrId("lc-sign-output-folder-error-message"));
            return;
        }
    }

    // Defensive: re-validate TSA URL. FileSelectionPage already enforces
    // https+host for non-B_B levels via signing::isValidTsaUrl, but
    // configure() could be called directly with anything — re-check here
    // so the signing service is never handed an untrusted URL.
    if (sigLevel != QStringLiteral("B_B") && !tsaUrl.empty()) {
        if (!signing::isValidTsaUrl(QString::fromStdString(tsaUrl))) {
            QMessageBox::warning(this, qtTrId("lc-sign-tsa-invalid-title"), qtTrId("lc-sign-tsa-invalid-message"));
            return;
        }
    }

    // M4: Warn if certificate is expired and level is B-B (no revocation data embedded)
    if (sigLevel == QStringLiteral("B_B") && signing::isCertificateExpired(certificate.derBytes)) {
        auto answer =
            QMessageBox::warning(this, qtTrId("lc-sign-expired-cert-title"), qtTrId("lc-sign-expired-cert-message"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    signingInProgress = true;
    emit signingStarted();

    // Read PIN + CAN into LibreSCRS::Secure::String BEFORE hiding the input
    // rows. canRow->isVisible() is the CL-reader gate for the CAN:PIN path;
    // hiding the row first would short-circuit the check and send only the
    // raw PIN, breaking contactless signing. Moving this read up also
    // shortens the window PIN material lives inside the QLineEdit's QString
    // storage.
    //
    // The Secure::String(std::string&&) constructor adopts the rvalue from
    // QString::toStdString() and cleanses its bytes (including the SSO
    // inline buffer) before the rvalue dies, so no plaintext intermediate
    // outlives the assignment.
    LibreSCRS::Secure::String pinSecure{pinEdit->text().toStdString()};
    LibreSCRS::Secure::String canSecure;
    if (canRow->isVisible() && !canEdit->text().isEmpty())
        canSecure = LibreSCRS::Secure::String{canEdit->text().toStdString()};

    // PIN and CAN values are now safely in SecureBuffer. Hide the input
    // rows so the progress + results area owns the bottom half of the
    // page; the widgets have no further role during signing or on the
    // completion screen.
    pinRow->setVisible(false);
    canRow->setVisible(false);
    progressWidget->setVisible(true);
    progressBar->setRange(0, 0); // indeterminate
    progressLabel->setText(qtTrId("lc-sign-preparing"));
    resultsList->setVisible(true);

    const int totalFiles = fileInfos.count();

    const QString pkcs11Path = signing::findPkcs11Module();
    const std::string keyAlias = certificate.label;
    const LibreSCRS::Signing::SignatureLevel level = parseLevel(sigLevel);

    struct SignJob
    {
        QString filePath;
        QString outputPath;
        LibreSCRS::Signing::SignatureFormat format;
        LibreSCRS::Signing::PackagingMode packaging;
    };

    QList<SignJob> jobs;
    for (const auto& info : fileInfos)
        jobs.append({info.filePath, buildOutputPath(info), info.format, info.packaging});

    auto svc = signingService;    // outlives wizard (owned by LibreCelik)
    auto plugin = cardPlugin;     // shared — worker lambda retains ownership
    auto activeSession = session; // shared — worker lambda retains ownership
    auto visualTemplate = std::move(visualParams);
    auto tsaUrlCopy = tsaUrl;
    QPointer<SignPage> guard(this);

    // Per-request TSA override: if the wizard's FileSelectionPage supplied a
    // specific TSA URL (shown only for B-T and higher), each SigningRequest
    // carries its own tsaOverride. The shared SigningService's service-level
    // TsaProvider (installed once at startup from SigningConfiguration) is
    // the fallback when no override is set — no service-wide state mutation,
    // no RAII restore.
    workerThread = QThread::create([this, guard, jobs, pinSecure = std::move(pinSecure),
                                    canSecure = std::move(canSecure), keyAlias, level, totalFiles,
                                    visualTemplate = std::move(visualTemplate), svc, plugin,
                                    session = std::move(activeSession), tsaUrl = std::move(tsaUrlCopy)]() mutable {
        int succeeded = 0;
        int failed = 0;
        try {
            // Hand the CAN to the plugin BEFORE the per-file loop so PACE
            // runs once per signing session rather than once per file. The
            // plugin stores its own cleansing copy; the worker-local
            // canSecure cleanses on scope exit.
            if (!canSecure.empty())
                plugin->setCredentials(*session, "can", canSecure);

            for (int i = 0; i < jobs.count(); ++i) {
                const auto& job = jobs.at(i);
                QFileInfo fi(job.filePath);

                QMetaObject::invokeMethod(
                    guard.data(), [guard, this, i, count = jobs.count(), name = fi.fileName(), totalFiles]() {
                        if (!guard)
                            return;
                        progressLabel->setText(qtTrId("lc-sign-progress").arg(i + 1).arg(count).arg(name));
                        if (totalFiles > 1) {
                            progressBar->setRange(0, totalFiles);
                            progressBar->setValue(i);
                        } else {
                            progressBar->setRange(0, 0);
                        }
                    });

                // Pre-flight: refuse files larger than 256 MiB without touching the bridge.
                constexpr qint64 maxFileSize = 256 * 1024 * 1024;
                if (fi.size() > maxFileSize) {
                    QMetaObject::invokeMethod(guard.data(), [guard, this, name = fi.fileName()]() {
                        if (!guard)
                            return;
                        addResultItem(QStringLiteral("\u2718"), signing::kErrorHex,
                                      qtTrId("lc-sign-fail-too-large").arg(name));
                    });
                    ++failed;
                    continue;
                }

                LibreSCRS::Signing::SigningRequest::Builder rb;
                rb.inputFile(std::filesystem::path(job.filePath.toStdString()));
                rb.outputFile(std::filesystem::path(job.outputPath.toStdString()));
                rb.format(job.format);
                rb.packaging(job.packaging);
                rb.level(level);
                rb.certificateLabel(keyAlias);
                if (!tsaUrl.empty())
                    rb.tsaOverride(LibreSCRS::Signing::staticTsa(tsaUrl));
                if (job.format == LibreSCRS::Signing::SignatureFormat::Pades && visualTemplate.has_value())
                    rb.visualParams(cloneWithRefreshedTimestamp(*visualTemplate));

                LibreSCRS::Signing::SigningRequest req = std::move(rb).build();

                // CredentialProvider: hands the signing PIN back in the
                // "pin" field that AuthRequirement::forSigning declares. The
                // CAN, when present, was installed on the plugin before the
                // loop via setCredentials — it is not a signing credential.
                //
                // pinSecure is captured by reference; each invocation hands
                // out a fresh per-copy-cleansing duplicate via Secure::String
                // copy semantics. The CredentialResult-owned copy cleanses
                // when the result goes out of scope.
                auto credentialProvider =
                    [&pinSecure](const LibreSCRS::Auth::AuthRequirement&) -> LibreSCRS::Auth::CredentialResult {
                    std::vector<LibreSCRS::Auth::CredentialResult::Entry> values;
                    values.emplace_back("pin", pinSecure);
                    return LibreSCRS::Auth::CredentialResult::ok(std::move(values));
                };

                LibreSCRS::Signing::SigningResult result = svc->sign(req, credentialProvider, plugin, session);

                using S = LibreSCRS::Signing::SigningResult::Status;
                if (result.status == S::Ok) {
                    ++succeeded;
                    QMetaObject::invokeMethod(guard.data(), [guard, this, name = fi.fileName(),
                                                             outName = QFileInfo(job.outputPath).fileName()]() {
                        if (!guard)
                            return;
                        addResultItem(QStringLiteral("\u2714"), signing::kSuccessHex,
                                      qtTrId("lc-sign-ok").arg(name, outName));
                    });
                } else {
                    ++failed;
                    QString msg;
                    // Track whether the PIN pathway is now compromised so the
                    // outer loop stops issuing further sign() calls. Every
                    // subsequent call would re-send the same stored PIN and
                    // consume another on-card retry (3 retries → permanent
                    // block).
                    bool pinCompromised = false;
                    switch (result.status) {
                    case S::UserCancelled:
                        msg = qtTrId("lc-sign-fail-cancelled");
                        break;
                    case S::PinVerificationFailed:
                        msg = qtTrId("lc-sign-fail-pin");
                        pinCompromised = true;
                        break;
                    case S::CardBlocked:
                        msg = qtTrId("lc-sign-fail-blocked");
                        pinCompromised = true;
                        break;
                    case S::TsaUnreachable:
                        msg = qtTrId("lc-sign-fail-tsa");
                        break;
                    case S::TrustStoreUnavailable:
                        msg = qtTrId("lc-sign-fail-trust");
                        break;
                    case S::InvalidRequest:
                        msg = qtTrId("lc-sign-fail-invalid");
                        break;
                    case S::SigningEngineError:
                    default:
                        msg = result.diagnosticDetail.has_value()
                                  ? qtTrId("lc-sign-fail-sign")
                                        .arg(fi.fileName(), QString::fromStdString(*result.diagnosticDetail))
                                  : qtTrId("lc-sign-fail-generic");
                        break;
                    }
                    QMetaObject::invokeMethod(guard.data(), [guard, this, name = fi.fileName(), msg]() {
                        if (!guard)
                            return;
                        addResultItem(QStringLiteral("\u2718"), signing::kErrorHex, msg);
                        Q_UNUSED(name);
                    });
                    if (pinCompromised) {
                        // Record every remaining job as failed with the same
                        // PIN-related diagnostic and halt the loop. The user
                        // will see one failure per file plus the original
                        // PIN-failed entry; the card retry counter is only
                        // burned once for the batch.
                        const bool wasBlocked = (result.status == S::CardBlocked);
                        for (int j = i + 1; j < jobs.count(); ++j) {
                            const auto& skipJob = jobs.at(j);
                            QFileInfo skipFi(skipJob.filePath);
                            ++failed;
                            const QString skipMsg =
                                wasBlocked ? qtTrId("lc-sign-fail-blocked") : qtTrId("lc-sign-fail-pin");
                            QMetaObject::invokeMethod(guard.data(), [guard, this, name = skipFi.fileName(), skipMsg]() {
                                if (!guard)
                                    return;
                                addResultItem(QStringLiteral("\u2718"), signing::kErrorHex, skipMsg);
                                Q_UNUSED(name);
                            });
                        }
                        break;
                    }
                }
            }

        } catch (const std::exception& e) {
            // Count any jobs we didn't reach as failed, and show the error
            // to the user. The outer loop has already incremented succeeded
            // / failed for jobs that ran to completion; anything left over
            // never ran.
            const int totalCount = static_cast<int>(jobs.count());
            const int unreached = totalCount - succeeded - failed;
            if (unreached > 0)
                failed += unreached;
            QMetaObject::invokeMethod(guard.data(), [guard, this, err = QString::fromUtf8(e.what())]() {
                if (!guard)
                    return;
                addResultItem(QStringLiteral("\u2718"), signing::kErrorHex, err);
            });
        } catch (...) {
            const int totalCount = static_cast<int>(jobs.count());
            const int unreached = totalCount - succeeded - failed;
            if (unreached > 0)
                failed += unreached;
            QMetaObject::invokeMethod(guard.data(), [guard, this]() {
                if (!guard)
                    return;
                //% "Unknown signing error"
                addResultItem(QStringLiteral("\u2718"), signing::kErrorHex, qtTrId("lc-sign-unknown-error"));
            });
        }

        // PIN cleansing happens unconditionally via PinScrubber RAII at lambda exit.

        // Final update on main thread
        QMetaObject::invokeMethod(guard.data(), [guard, this, succeeded, failed, count = jobs.count()]() {
            if (!guard)
                return;
            progressBar->setRange(0, count);
            progressBar->setValue(count);
            progressLabel->setText(qtTrId("lc-sign-complete").arg(succeeded).arg(failed));
            clearPin();
            signingInProgress = false;
            signingComplete = true;
            failedCount = failed;
            emit signingFinished(succeeded, failed);
        });
    });

    connect(workerThread, &QThread::finished, workerThread, &QThread::deleteLater);
    connect(workerThread, &QThread::finished, this, [this]() { workerThread = nullptr; });
    workerThread->start();
}

QString SignPage::buildOutputPath(const FileSignInfo& info) const
{
    QFileInfo fi(info.filePath);
    QString outputPath;
    switch (info.format) {
    case LibreSCRS::Signing::SignatureFormat::Pades:
        outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral("-signed.pdf"));
        break;
    case LibreSCRS::Signing::SignatureFormat::Xades:
        if (info.packaging == LibreSCRS::Signing::PackagingMode::Enveloped)
            outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral("-signed.xml"));
        else
            outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".xsig"));
        break;
    case LibreSCRS::Signing::SignatureFormat::AsicE:
        outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral(".asice"));
        break;
    case LibreSCRS::Signing::SignatureFormat::Cades:
        outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".p7s"));
        break;
    case LibreSCRS::Signing::SignatureFormat::Jades:
        outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".jose"));
        break;
    }
    if (QFile::exists(outputPath)) {
        QFileInfo outFi(outputPath);
        QString base = outFi.completeBaseName();
        QString suffix = outFi.suffix();
        QString dir = outFi.absolutePath();
        // Cap the search: if the user somehow has 10000+ numbered siblings,
        // bail with an empty path so the caller's open-for-write failure
        // surfaces a normal error rather than us spinning forever.
        constexpr int kMaxSuffixAttempts = 10000;
        int counter = 2;
        do {
            outputPath = QDir(dir).filePath(QStringLiteral("%1(%2).%3").arg(base).arg(counter).arg(suffix));
            ++counter;
        } while (QFile::exists(outputPath) && counter <= kMaxSuffixAttempts);
        if (QFile::exists(outputPath))
            return {};
    }
    return outputPath;
}

LibreSCRS::Signing::SignatureLevel SignPage::parseLevel(const QString& level)
{
    using L = LibreSCRS::Signing::SignatureLevel;
    if (level == QStringLiteral("B_B"))
        return L::B_B;
    if (level == QStringLiteral("B_LT"))
        return L::B_LT;
    if (level == QStringLiteral("B_LTA"))
        return L::B_LTA;
    return L::B_T;
}

void SignPage::addResultItem(const QString& icon, const QString& colorHex, const QString& message)
{
    auto* item = new QListWidgetItem(resultsList);
    item->setData(ResultDelegate::IconTextRole, icon);
    item->setData(ResultDelegate::IconColorRole, colorHex);
    item->setData(ResultDelegate::MessageRole, message);
}

void SignPage::clearPin()
{
    // Best-effort PIN clearing: fill QLineEdit text with null characters,
    // then clear. Qt's internal QString storage is not directly controllable,
    // but this overwrites the visible text in the widget's buffer.
    int len = pinEdit->text().size();
    pinEdit->setText(QString(len, QChar(0)));
    pinEdit->clear();

    if (canEdit) {
        int canLen = canEdit->text().size();
        canEdit->setText(QString(canLen, QChar(0)));
        canEdit->clear();
    }
}
