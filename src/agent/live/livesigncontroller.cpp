// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/live/livesigncontroller.h"

#include "agent/artifactio.h"
#include "agent/errortext.h"
#include "agent/opstallwatchdog.h"
#include "agent/signrequest.h"

#include <LibreSCRS/AgentClient/AgentCard.h>
#include <LibreSCRS/AgentClient/AgentClient.h>
#include <LibreSCRS/AgentClient/AgentOperation.h>
#include <LibreSCRS/AgentClient/ClientTimeouts.h>

#include <QByteArray>
#include <QFileInfo>
#include <QLatin1StringView>
#include <QSaveFile>

#include <cerrno>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <unistd.h>

namespace librecelik::agent {

using LibreSCRS::AgentClient::AgentCard;
using LibreSCRS::AgentClient::AgentOperation;
using LibreSCRS::AgentClient::BatchDocument;
using LibreSCRS::AgentClient::BatchSignRow;
using LibreSCRS::AgentClient::CallError;
using LibreSCRS::AgentClient::ErrorCode;
using LibreSCRS::AgentClient::FdHandle;
using LibreSCRS::AgentClient::OperationStatus;
using LibreSCRS::AgentClient::SignOptions;

namespace {

/// The feature tokens this controller gates on. Spelled once here, next to the
/// three capability accessors that are their only readers.
constexpr QLatin1StringView kVisualSignFeature{"visual-sign"};
constexpr QLatin1StringView kLayoutPreviewFeature{"layout-preview"};
constexpr QLatin1StringView kTsaUrlFeature{"tsa-url"};
constexpr QLatin1StringView kBatchSignFeature{"batch-sign"};

/// A watchdog armed on one operation, plus the flag its expiry sets.
///
/// Deliberately the same shape the card controller arms (`livecardcontroller.cpp`)
/// rather than a shared helper: the two are the only arming sites, and the
/// flag is shared rather than owned because it outlives the expiry it records —
/// the operation's terminal arrives LATER (a cancel is fire-and-forget), and by
/// then this flag is the only evidence the terminal was a stall.
struct OpWatch
{
    OpStallWatchdog* dog = nullptr;
    std::shared_ptr<bool> fired;
};

/// Arm a per-operation stall bound: parented to the operation (so it cannot
/// outlive it), fed by the operation's own phase stream, cancelling on expiry.
/// Phase-aware by construction — the consent and authentication phases, where
/// the human is at the prompter, never time out; a timestamp leg is a long
/// MACHINE phase and restarts the budget on every tick.
[[nodiscard]] OpWatch armWatchdog(AgentOperation* operation)
{
    auto* dog = new OpStallWatchdog(LibreSCRS::AgentClient::kLongOperationTimeoutMs, operation);
    auto fired = std::make_shared<bool>(false);
    QObject::connect(operation, &AgentOperation::phaseChanged, dog, &OpStallWatchdog::onPhase);
    QObject::connect(dog, &OpStallWatchdog::expired, operation, [operation, fired] {
        *fired = true;
        // Fire-and-forget by contract: the terminal still arrives via
        // finished(), which is the only place an outcome is ever read.
        operation->cancel();
    });
    dog->begin();
    return OpWatch{dog, std::move(fired)};
}

/// The line for a verb issued against a card the client has already removed.
[[nodiscard]] QString cardGoneText()
{
    return errorText(ErrorCode::CardRemoved, CallError::None, {}, {});
}

/// The line for an option this LC offered but the connected agent cannot
/// honour — the same refusal the client itself makes locally, said in LC's
/// voice because LC refuses BEFORE dialling.
[[nodiscard]] QString capabilityMissingText()
{
    return errorText(ErrorCode::CapabilityMissing, CallError::None, {}, {});
}

/// Split every run into one-document runs, preserving each row's
/// selection-order index. The degrade path for an agent without batch signing:
/// N documents become N `sign()` calls with N prompts — exactly today's
/// per-file behaviour — rather than a refusal that would cost the user
/// multi-file signing entirely.
[[nodiscard]] QList<SignRun> splitIntoSingleRuns(const QList<SignRun>& runs)
{
    QList<SignRun> singles;
    for (const SignRun& run : runs) {
        for (int position = 0; position < run.items.size(); ++position) {
            singles.append(
                SignRun{QList<int>{run.sourceIndexes.at(position)}, QList<SignRequestItem>{run.items.at(position)}});
        }
    }
    return singles;
}

} // namespace

LiveSignController::LiveSignController(AgentCard* cardProxy, LibreSCRS::AgentClient::AgentClient* agentClient,
                                       QObject* parent)
    : SignController(parent), card(cardProxy), client(agentClient)
{}

LiveSignController::~LiveSignController() = default;

bool LiveSignController::canVisualSign() const
{
    // Stricter than the client's own dial gate on purpose: LC refuses to OFFER
    // a visual signature it cannot PREVIEW, so the placement page and the
    // signed output can never disagree.
    return client != nullptr && client->hasFeature(kVisualSignFeature) && client->hasFeature(kLayoutPreviewFeature);
}

bool LiveSignController::canTsaOverride() const
{
    return client != nullptr && client->hasFeature(kTsaUrlFeature);
}

bool LiveSignController::canBatch() const
{
    return client != nullptr && client->hasFeature(kBatchSignFeature);
}

void LiveSignController::start(const QString& requestCertId, const QList<SignRequestItem>& requestFiles,
                               const SignOptions& requestOptions, const QString& requestOutputFolder)
{
    if (running) {
        // A second request while one is in flight would put a second consent in
        // front of a user already answering the first. The wizard drives one
        // request at a time; this is the belt to that brace.
        return;
    }

    certId = requestCertId;
    files = requestFiles;
    options = requestOptions;
    outputFolder = requestOutputFolder;
    runs.clear();
    nextRun = 0;
    succeeded = 0;
    failed = 0;
    running = true;
    cancelRequested = false;

    // The wire's frozen batch bound, asserted here as well as in the wizard:
    // the client refuses an out-of-bounds call at entry, and a refusal the user
    // can read per file beats an entry error attached to nothing.
    const auto count = static_cast<std::size_t>(files.size());
    if (count < LibreSCRS::AgentClient::kMinBatchDocuments || count > LibreSCRS::AgentClient::kMaxBatchDocuments) {
        const QString message = errorText(ErrorCode::None, CallError::InvalidArguments, {}, {});
        for (int index = 0; index < files.size(); ++index) {
            emitRow(index, false, {}, message);
        }
        finishRequest();
        return;
    }

    // Capability gates, mirroring the client's own local-refusal posture: LC
    // never dials an option the connected agent has not advertised, so the
    // failure is a localized line rather than a wire round trip that ends in
    // CapabilityMissing.
    if ((!options.visualSignature.isEmpty() && !canVisualSign()) || (!options.tsaUrl.isEmpty() && !canTsaOverride())) {
        const QString message = capabilityMissingText();
        for (int index = 0; index < files.size(); ++index) {
            emitRow(index, false, {}, message);
        }
        finishRequest();
        return;
    }

    runs = partitionIntoRuns(files);
    if (!canBatch()) {
        runs = splitIntoSingleRuns(runs);
    }
    dispatchNextRun();
}

void LiveSignController::dispatchNextRun()
{
    while (nextRun < runs.size()) {
        const SignRun run = runs.at(nextRun); // by value: the handler outlives this frame
        ++nextRun;

        if (card.isNull()) {
            failRun(run, cardGoneText());
            continue;
        }

        // Open every document first. A file that cannot be read fails right
        // here, before any consent: it must never cost the user a PIN
        // presentation for a run it could not have completed anyway.
        SignRun dialled;
        std::vector<BatchDocument> documents;
        for (int position = 0; position < run.items.size(); ++position) {
            const SignRequestItem& item = run.items.at(position);
            FdHandle document = openDocumentFd(item.filePath);
            if (!document.valid()) {
                emitRow(run.sourceIndexes.at(position), false, {},
                        errorText(ErrorCode::InvalidDocument, CallError::None, {}, {}));
                continue;
            }
            dialled.sourceIndexes.append(run.sourceIndexes.at(position));
            dialled.items.append(item);
            // displayName feeds the agent's enumerated consent prompt — the
            // user must recognise what they are about to sign.
            documents.push_back(BatchDocument{QFileInfo(item.filePath).fileName(), std::move(document)});
        }
        if (documents.empty()) {
            continue; // nothing left in this run to sign
        }

        SignOptions runOptions = options;
        // EXPLICIT per-run kind. A batch carries ONE format for every document
        // in it and the wire sniffs only the first, so a run's kind is set from
        // the run itself — never inferred, never left to a batch-wide guess
        // that would apply one file's kind to all the others.
        runOptions.format = dialled.items.constFirst().format;
        runOptions.packaging = dialled.items.constFirst().packaging;

        const bool batched = documents.size() > 1;
        AgentOperation* operation = batched ? card->signBatch(certId, std::move(documents), runOptions)
                                            : card->sign(certId, std::move(documents.front().fd), runOptions);
        inFlight = operation;
        connect(operation, &AgentOperation::phaseChanged, this, &SignController::phaseChanged);
        const OpWatch watch = armWatchdog(operation);

        connect(operation, &AgentOperation::finished, this, [this, operation, watch, batched, dialled] {
            // FIRST, always: a normally completed run must never leave a timer
            // alive to cancel an operation that is already done.
            watch.dog->stop();
            inFlight.clear();
            consumeFinished(operation, watch.fired && *watch.fired, batched, dialled);
            operation->deleteLater();
        });
        return; // one run in flight at a time
    }
    finishRequest();
}

void LiveSignController::consumeFinished(AgentOperation* operation, bool watchdogFired, bool batched,
                                         const SignRun& run)
{
    // A caller-enforced stall has no wire error of its own — the operation
    // reports Cancelled with no code — so the agent's own watchdog code is the
    // honest name for it, and it reads as retryable rather than final.
    const ErrorCode opError = watchdogFired ? ErrorCode::WatchdogTimeout : operation->errorCode();
    // The richest line available for a run-level failure: only here are the
    // agent-authored message and the transport axis in scope (the pure
    // consumption below sees the error-code axis alone).
    const QString opText = watchdogFired ? errorText(ErrorCode::WatchdogTimeout, CallError::None, {}, {})
                                         : errorText(operation->errorCode(), operation->callError(),
                                                     operation->messageKey(), operation->messageFallback());

    // ALWAYS taken, terminal or not: the wire delivers rows for every document
    // it attempted, including on a Finished(Error).
    std::vector<BatchSignRow> rows = operation->takeBatchResults();
    // Wire-answered rows ONLY — counted BEFORE the single-sign synthesis
    // below. The synthetic row carries the error-code axis alone, so a
    // single sign that failed without a wire answer (an entry refusal above
    // all) must fall through to opText, the only spelling that still has the
    // call axis and the agent's authored message.
    const auto answered = static_cast<std::size_t>(rows.size());
    if (!batched) {
        // A single sign() answers one artifact through its own accessor. It
        // enters the SAME consumption as a one-row batch so there is exactly
        // one place where "which rows succeeded" is decided.
        BatchSignRow row;
        row.displayName = run.items.isEmpty() ? QString() : QFileInfo(run.items.constFirst().filePath).fileName();
        row.artifact = operation->takeSignedArtifact();
        row.error = opError;
        // signMeta() is deliberately not rendered anywhere: its chainComplete
        // is reserved-always-false, and the rest is log material.
        row.meta = operation->signMeta();
        rows.push_back(std::move(row));
    }

    BatchOutcome outcome = consumeBatchOutcome(operation->status(), opError, std::move(rows), run);

    QString lastFailure;
    for (std::size_t position = 0; position < outcome.emissions.size(); ++position) {
        RowEmission& emission = outcome.emissions[position];
        const SignRequestItem& item = files.at(emission.sourceIndex);
        if (emission.ok) {
            const QString written = writeArtifact(item, std::move(emission.artifact));
            if (written.isEmpty()) {
                lastFailure = artifactWriteFailedText();
                emitRow(emission.sourceIndex, false, {}, lastFailure);
            } else {
                emitRow(emission.sourceIndex, true, written, {});
            }
            continue;
        }
        // A row the wire never answered for reports the operation's own
        // failure, in the richer spelling only this scope can build.
        lastFailure = position >= answered ? opText : emission.message;
        emitRow(emission.sourceIndex, false, {}, lastFailure);
    }

    if (cancelRequested) {
        // The runs never dialled are reported with the same reason the
        // cancelled run's own rows carry — a cancelled request must not leave
        // rows the wizard never hears about.
        failRemainingRuns(lastFailure.isEmpty() ? opText : lastFailure);
        finishRequest();
        return;
    }
    if (outcome.haltRemainingRuns) {
        // The halt is contagious ACROSS runs too: the credential is wrong or
        // blocked, and every further run would present it again and spend
        // another on-card retry. The halt's own text is on the run's last
        // emission (the outcome carries no separate field for it).
        failRemainingRuns(lastFailure.isEmpty() ? opText : lastFailure);
        finishRequest();
        return;
    }
    if (outcome.retryableRateLimit) {
        // Every call is its own rate-limiter charge, so the next run would be
        // charged too. Surface it and stop — the user re-runs when ready, and
        // nothing here ever retries on its own.
        failRemainingRuns(errorText(ErrorCode::RateLimited, CallError::None, {}, {}));
        finishRequest();
        return;
    }
    dispatchNextRun();
}

QString LiveSignController::writeArtifact(const SignRequestItem& item, FdHandle artifact)
{
    const QString path = outputPathFor(item, outputFolder);
    if (path.isEmpty() || !artifact.valid()) {
        return {};
    }
    // The tested consumer (artifactio.h): rewinds the fd the producer left at
    // EOF, and refuses to commit a zero-byte artifact as a signed file.
    return librecelik::agent::writeArtifactTo(artifact.get(), path);
}

void LiveSignController::emitRow(int sourceIndex, bool ok, const QString& outputPath, const QString& message)
{
    if (ok) {
        ++succeeded;
    } else {
        ++failed;
    }
    SignRowResult result;
    result.filePath = files.at(sourceIndex).filePath;
    result.outputPath = outputPath;
    result.ok = ok;
    result.message = message;
    Q_EMIT rowFinished(sourceIndex, result);
}

void LiveSignController::failRun(const SignRun& run, const QString& message)
{
    for (const int sourceIndex : run.sourceIndexes) {
        emitRow(sourceIndex, false, {}, message);
    }
}

void LiveSignController::failRemainingRuns(const QString& message)
{
    while (nextRun < runs.size()) {
        failRun(runs.at(nextRun), message);
        ++nextRun;
    }
}

void LiveSignController::finishRequest()
{
    running = false;
    Q_EMIT finished(succeeded, failed);
}

void LiveSignController::cancel()
{
    if (!running) {
        return;
    }
    // Whatever has not been dialled yet never will be: a cancelled request must
    // not cost another consent for the runs behind it.
    cancelRequested = true;
    if (!inFlight.isNull()) {
        // Fire-and-forget: the terminal still arrives through finished(), which
        // is where the outcome is read — the rows the agent already produced
        // are written and reported there, never dropped here.
        inFlight->cancel();
        return;
    }
    // No operation is carrying a terminal for us (nothing was dialled), so the
    // remaining rows are reported here, with the only line available: nothing
    // ran, and nothing reported a reason.
    failRemainingRuns(errorText(ErrorCode::None, CallError::None, {}, {}));
    finishRequest();
}

} // namespace librecelik::agent
