// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "signpage.h"

#include "certutils.h"
#include "pkcs11utils.h"
#include "resultdelegate.h"
#include "signingcolors.h"
#include "utils/iconutils.h"

#include <libresign/signing_service.h>
#include <smartcard/secure_buffer.h>

#include <QAction>
#include <QDateTime>
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

#include <openssl/crypto.h>

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
    // of QIntValidator, which overflows for maxLength > 9 (see memory:
    // feedback_pin_validator_overflow).
    canEdit->setMaxLength(10);
    canEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d*$")), canEdit));
    canRowLayout->addWidget(canEdit);
    canRowLayout->addStretch();
    canRow->setVisible(false); // shown only for CL readers
    layout->addWidget(canRow);

    // --- PIN entry ---
    auto* pinRow = new QHBoxLayout;
    //% "PIN:"
    pinLabel = new QLabel(qtTrId("lc-sign-pin-label"), this);
    pinRow->addWidget(pinLabel);

    pinEdit = new QLineEdit(this);
    pinEdit->setEchoMode(QLineEdit::Password);
    pinEdit->setMaximumWidth(160);
    pinEdit->setMaxLength(256);
    pinEdit->setAttribute(Qt::WA_InputMethodEnabled, false);
    iconutils::addToggleVisibilityAction(this, pinEdit);
    pinRow->addWidget(pinEdit);
    pinRow->addStretch();
    layout->addLayout(pinRow);

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
    // cancellable mid-call, so if the module misbehaves we accept leaking the
    // worker thread (the OS reaps it on process exit) rather than freezing
    // the entire app. The PIN material is in a smartcard::SecureBuffer captured
    // by the lambda, so the leaked thread does NOT leave PIN bytes around
    // indefinitely either — the SecureBuffer destructor still cleanses when
    // the thread eventually returns.
    if (workerThread && workerThread->isRunning()) {
        workerThread->disconnect(this);
        constexpr int kShutdownWaitMs = 5000; // 5s — matches typical PKCS#11 sign latency
        if (!workerThread->wait(kShutdownWaitMs)) {
            qWarning("SignPage: worker thread did not finish within %dms; "
                     "leaking thread to avoid blocking shutdown",
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

void SignPage::configure(const Config& cfg)
{
    certificate = cfg.certificate;
    readerName = cfg.readerName;
    fileInfos = cfg.fileInfos;
    sigLevel = cfg.level;
    outputDir = cfg.outputFolder;
    visualParams = cfg.visual;
    tsaUrl = cfg.tsaUrl;
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

void SignPage::setSigningService(libresign::SigningService* svc)
{
    signingService = svc;
}

void SignPage::setTrustConfig(const libresign::TrustConfig& config)
{
    trustConfig = config;
}

void SignPage::setPrefetchCallback(std::function<bool()> cb)
{
    prefetchCallback = std::move(cb);
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

void SignPage::startSigning()
{
    if (workerThread && workerThread->isRunning())
        return;

    if (!signingService || fileInfos.isEmpty())
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
            QMessageBox::warning(this, qtTrId("lc-sign-tsa-invalid-title"),
                                 qtTrId("lc-sign-tsa-invalid-message"));
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

    pinEdit->setEnabled(false);
    progressWidget->setVisible(true);
    progressBar->setRange(0, 0); // indeterminate
    progressLabel->setText(qtTrId("lc-sign-preparing"));
    resultsList->setVisible(true);

    const int totalFiles = fileInfos.count();

    const QString pkcs11Path = signing::findPkcs11Module();

    // Build PIN buffer using smartcard::SecureBuffer (RAII zeroization).
    // Intermediate std::strings from QString::toStdString() are explicitly
    // cleansed before they leave scope so PIN material doesn't leak through
    // the QString → std::string → SecureBuffer conversion path.
    smartcard::SecureBuffer pinBuffer;
    {
        std::string pinTmp;
        if (canRow->isVisible() && !canEdit->text().isEmpty()) {
            std::string can = canEdit->text().toStdString();
            std::string pin = pinEdit->text().toStdString();
            // Build CAN:PIN in-place to avoid `operator+` temporaries that
            // would briefly hold the concatenated secret outside our control
            // and destruct without cleanse. reserve() guarantees no further
            // allocation during the build.
            pinTmp.reserve(can.size() + 1 + pin.size());
            pinTmp.assign(can);
            pinTmp.push_back(':');
            pinTmp.append(pin);
            OPENSSL_cleanse(can.data(), can.size());
            OPENSSL_cleanse(pin.data(), pin.size());
        } else {
            pinTmp = pinEdit->text().toStdString();
        }
        pinBuffer = smartcard::SecureBuffer(pinTmp);
        OPENSSL_cleanse(pinTmp.data(), pinTmp.size());
    }
    const std::string keyAlias = certificate.label;
    const libresign::SignatureLevel level = parseLevel(sigLevel);

    // Capture parameters for worker thread
    struct SignJob
    {
        QString filePath;
        QString outputPath;
        libresign::SignatureFormat format;
        libresign::SignaturePackaging packaging;
    };

    QList<SignJob> jobs;
    for (const auto& info : fileInfos)
        jobs.append({info.filePath, buildOutputPath(info), info.format, info.packaging});

    // Run signing on worker thread to keep UI responsive.
    // A QPointer guard is captured so that QMetaObject::invokeMethod calls are
    // silently skipped if the SignPage is destroyed before they execute.
    auto visual = visualParams;
    auto tsa = tsaUrl;
    auto trust = trustConfig;
    auto prefetch = prefetchCallback;
    auto* svc = signingService; // outlives wizard (owned by LibreCelik)
    QPointer<SignPage> guard(this);
    // NOTE: `this` is captured for QMetaObject::invokeMethod inner lambdas that
    // access member functions (addResultItem, clearPin, etc.) and member variables.
    // Every inner lambda also captures the QPointer `guard` and checks `if (!guard)`
    // before dereferencing `this`, so the raw pointer is never used after destruction.
    // The outer lambda itself only uses `this` indirectly through those guard-protected
    // inner lambdas — all direct work is done through value-captured locals (svc, jobs, etc.).
    workerThread = QThread::create([this, guard, jobs, pkcs11Path, pinBuffer = std::move(pinBuffer), keyAlias, level,
                                    totalFiles, visual, tsa, trust, prefetch, svc]() mutable {
        // pinBuffer (smartcard::SecureBuffer) self-cleanses on destruction
        // regardless of how this lambda exits — including via exception
        // thrown by svc->sign(). No additional scrubber needed.

        int succeeded = 0;
        int failed = 0;
        // Wrap the entire worker body so an uncaught exception from
        // svc->configure() / svc->sign() / anywhere downstream surfaces as
        // a visible failure in the UI instead of silently leaving the
        // progress bar spinning and the Sign button disabled.
        try {
        bool prefetched = prefetch ? prefetch() : false;

        if (!prefetched && !trust.trustedLists.empty()) {
            svc->configure(trust);
        }

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
                        progressBar->setRange(0, 0); // indeterminate for single file
                    }
                });

            // Open and validate size atomically on the open handle (avoid
            // TOCTOU between QFileInfo::size() and QFile::open() — and don't
            // read 256MB+ into RAM only to discard it).
            constexpr qint64 maxFileSize = 256 * 1024 * 1024;
            QFile file(job.filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                QMetaObject::invokeMethod(guard.data(), [guard, this, name = fi.fileName()]() {
                    if (!guard)
                        return;
                    addResultItem(QStringLiteral("\u2718"), signing::kErrorHex, qtTrId("lc-sign-fail-read").arg(name));
                });
                ++failed;
                continue;
            }
            if (file.size() > maxFileSize) {
                file.close();
                QMetaObject::invokeMethod(guard.data(), [guard, this, name = fi.fileName()]() {
                    if (!guard)
                        return;
                    addResultItem(QStringLiteral("\u2718"), signing::kErrorHex,
                                  qtTrId("lc-sign-fail-too-large").arg(name));
                });
                ++failed;
                continue;
            }
            const QByteArray data = file.readAll();
            if (file.error() != QFile::NoError) {
                file.close();
                QMetaObject::invokeMethod(guard.data(), [guard, this, name = fi.fileName()]() {
                    if (!guard)
                        return;
                    addResultItem(QStringLiteral("\u2718"), signing::kErrorHex, qtTrId("lc-sign-fail-read").arg(name));
                });
                ++failed;
                continue;
            }
            file.close();

            // Build request
            libresign::SigningRequest request;
            request.document = std::vector<uint8_t>(data.begin(), data.end());
            request.fileName = fi.fileName().toStdString();
            request.format = job.format;
            request.packaging = job.packaging;
            request.level = level;
            if (!tsa.empty())
                request.tsa.url = tsa;
            if (job.format == libresign::SignatureFormat::PAdES) {
                // Refresh the timestamp in the visual signature text so each
                // file gets the actual signing time rather than the stale
                // timestamp captured when the user entered the sign page.
                if (visual.enabled && !visual.text.empty()) {
                    QString text = QString::fromStdString(visual.text);
                    static const QRegularExpression dateRe(
                        QStringLiteral("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}"));
                    text.replace(dateRe,
                                 QDateTime::currentDateTime().toString(
                                     QStringLiteral("yyyy-MM-dd HH:mm:ss")));
                    visual.text = text.toStdString();
                }
                request.visual = visual;
            }
#ifdef LIBRESCRS_TESTING
            // Allow signing with expired certificates (test builds only)
            if (qEnvironmentVariableIsSet("LIBRESCRS_ALLOW_EXPIRED_CERT"))
                request.allowExpiredCertificate = true;
#endif

            // Sign
            auto result = svc->sign(request, pkcs11Path.toStdString(), pinBuffer, keyAlias);

            if (result.success) {
                QFile outFile(job.outputPath);
                const qint64 expectedBytes = static_cast<qint64>(result.signedDocument.size());
                bool wroteOk = false;
                if (outFile.open(QIODevice::WriteOnly)) {
                    const qint64 written = outFile.write(
                        reinterpret_cast<const char*>(result.signedDocument.data()), expectedBytes);
                    // Treat partial write or any QFile error as failure so we
                    // don't report a corrupt signed file as success.
                    wroteOk = (written == expectedBytes && outFile.error() == QFile::NoError);
                    outFile.close();
                    if (!wroteOk) {
                        // Best-effort cleanup of the truncated/garbage file.
                        outFile.remove();
                    }
                }
                if (wroteOk) {
                    QMetaObject::invokeMethod(guard.data(), [guard, this, name = fi.fileName(),
                                                             outName = QFileInfo(job.outputPath).fileName()]() {
                        if (!guard)
                            return;
                        addResultItem(QStringLiteral("\u2714"), signing::kSuccessHex,
                                      qtTrId("lc-sign-ok").arg(name, outName));
                    });
                    ++succeeded;
                } else {
                    QMetaObject::invokeMethod(guard.data(), [guard, this, name = fi.fileName()]() {
                        if (!guard)
                            return;
                        addResultItem(QStringLiteral("\u2718"), signing::kErrorHex,
                                      qtTrId("lc-sign-fail-write").arg(name));
                    });
                    ++failed;
                }
            } else {
                QMetaObject::invokeMethod(guard.data(), [guard, this, name = fi.fileName(),
                                                         err = QString::fromStdString(result.errorMessage)]() {
                    if (!guard)
                        return;
                    addResultItem(QStringLiteral("\u2718"), signing::kErrorHex,
                                  qtTrId("lc-sign-fail-sign").arg(name, err));
                });
                ++failed;
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
                addResultItem(QStringLiteral("\u2718"), signing::kErrorHex,
                              qtTrId("lc-sign-unknown-error"));
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
    case libresign::SignatureFormat::PAdES:
        outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral("-signed.pdf"));
        break;
    case libresign::SignatureFormat::XAdES:
        if (info.packaging == libresign::SignaturePackaging::ENVELOPED)
            outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral("-signed.xml"));
        else
            outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".xsig"));
        break;
    case libresign::SignatureFormat::ASiC_E:
        outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral(".asice"));
        break;
    case libresign::SignatureFormat::CAdES:
        outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".p7s"));
        break;
    case libresign::SignatureFormat::JAdES:
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

libresign::SignatureLevel SignPage::parseLevel(const QString& level)
{
    if (level == QStringLiteral("B_B"))
        return libresign::SignatureLevel::B_B;
    if (level == QStringLiteral("B_LT"))
        return libresign::SignatureLevel::B_LT;
    if (level == QStringLiteral("B_LTA"))
        return libresign::SignatureLevel::B_LTA;
    return libresign::SignatureLevel::B_T; // default
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
