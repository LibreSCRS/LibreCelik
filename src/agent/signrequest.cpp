// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/signrequest.h"

#include "agent/errortext.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLatin1StringView>
#include <QVariant>

#include <algorithm>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>

namespace librecelik::agent {

using LibreSCRS::AgentClient::BatchSignRow;
using LibreSCRS::AgentClient::CallError;
using LibreSCRS::AgentClient::ErrorCode;
using LibreSCRS::AgentClient::FdHandle;
using LibreSCRS::AgentClient::OperationStatus;
using LibreSCRS::AgentClient::Packaging;
using LibreSCRS::AgentClient::SignatureFormat;
using LibreSCRS::AgentClient::SignatureLevel;
using LibreSCRS::AgentClient::SignOptions;

namespace {

/// Byte length behind an artifact descriptor, or -1 when there is none to
/// measure. A failed row's descriptor is real and open, so its VALIDITY proves
/// nothing — only the length distinguishes it from a signed one.
[[nodiscard]] qint64 artifactSize(const FdHandle& artifact)
{
    if (!artifact.valid()) {
        return -1;
    }
    struct stat info{};
    if (::fstat(artifact.get(), &info) != 0) {
        return -1;
    }
    return static_cast<qint64>(info.st_size);
}

/// A HALTING failure: the signing credential itself was wrong or is blocked,
/// so the agent attempted nothing after it — and re-dialling would spend
/// another on-card retry against the same bad secret.
[[nodiscard]] bool isHaltingError(ErrorCode error)
{
    return error == ErrorCode::CredentialWrong || error == ErrorCode::CredentialBlocked;
}

} // namespace

SignatureLevel levelFromUiToken(const QString& token)
{
    if (token == QLatin1StringView("B_B")) {
        return SignatureLevel::BB;
    }
    if (token == QLatin1StringView("B_T")) {
        return SignatureLevel::BT;
    }
    if (token == QLatin1StringView("B_LT")) {
        return SignatureLevel::BLT;
    }
    if (token == QLatin1StringView("B_LTA")) {
        return SignatureLevel::BLTA;
    }
    // Anything else — an empty token, a token from a newer UI — defers to the
    // agent's configured default rather than guessing a level on the user's
    // behalf: guessing LOW silently produces a weaker signature than the
    // deployment is set up to produce.
    return SignatureLevel::Auto;
}

QVariantMap makeVisualSignatureMap(int page, const QRectF& box, const QString& text)
{
    QVariantMap map;
    // The wire types the page as an unsigned, so a negative page must not wrap
    // into an enormous one: it clamps to the first page, where a misplaced
    // visible signature is at least visible.
    map.insert(QStringLiteral("page"), static_cast<uint>(std::max(page, 0)));
    map.insert(QStringLiteral("x"), box.x());
    map.insert(QStringLiteral("y"), box.y());
    map.insert(QStringLiteral("width"), box.width());
    map.insert(QStringLiteral("height"), box.height());
    map.insert(QStringLiteral("text"), text);
    return map;
}

QList<SignRun> partitionIntoRuns(const QList<SignRequestItem>& files)
{
    QList<SignRun> runs;
    for (int index = 0; index < files.size(); ++index) {
        const SignRequestItem& item = files.at(index);
        // Keyed by kind, ordered by FIRST appearance: an interleaved selection
        // coalesces instead of alternating, because the number of consents (and
        // PIN presentations, and rate-limiter charges) a user pays must not
        // depend on the order the files happened to be picked in.
        const auto match = std::find_if(runs.begin(), runs.end(), [&item](const SignRun& run) {
            return run.items.constFirst().format == item.format && run.items.constFirst().packaging == item.packaging;
        });
        if (match == runs.end()) {
            runs.append(SignRun{QList<int>{index}, QList<SignRequestItem>{item}});
        } else {
            match->sourceIndexes.append(index);
            match->items.append(item);
        }
    }
    return runs;
}

QString documentDisplayName(const SignRequestItem& item)
{
    return QFileInfo(item.filePath).fileName();
}

SignOptions runSignOptions(const SignOptions& options, const SignRun& run)
{
    SignOptions dialled = options;
    dialled.format = run.items.constFirst().format;
    dialled.packaging = run.items.constFirst().packaging;
    if (run.items.size() == 1) {
        // Single sign only. The ASiC-E entry is named from this — a container
        // built without a name is refused, not silently unnamed — and the
        // detached JAdES/XAdES reference URI derives from it. A batch's rows
        // carry their own names and the agent never reads the option off a
        // SignBatch request, so setting it there would say nothing.
        dialled.displayName = documentDisplayName(run.items.constFirst());
    }
    return dialled;
}

bool batchRowSucceeded(ErrorCode rowError, qint64 artifactSize)
{
    // The wire gives rows no status field at all. Success is exactly "named no
    // error AND carries bytes": a failed row still resolves a real descriptor
    // (the pinned zero-length one), and a zero-length artifact is not a
    // signature however healthy the row looks.
    return rowError == ErrorCode::None && artifactSize > 0;
}

BatchOutcome consumeBatchOutcome(OperationStatus status, ErrorCode opError, std::vector<BatchSignRow> rows,
                                 const SignRun& run)
{
    // The terminal STATUS is deliberately not consulted per row: the wire
    // delivers rows on a Finished(Error) terminal too, and a row that named no
    // error and carries bytes was signed whatever the operation as a whole
    // reported. The row axis is the only verdict there is.
    Q_UNUSED(status)

    BatchOutcome outcome;
    outcome.retryableRateLimit = opError == ErrorCode::RateLimited;

    // What a row the wire never answered for reports: the OPERATION's own
    // failure. Such a document was never attempted, and dropping it silently
    // would lose a file from the user's result list.
    const QString unattemptedText = errorText(opError, CallError::None, {}, {});

    const qsizetype wanted = run.sourceIndexes.size();
    outcome.emissions.reserve(static_cast<std::size_t>(wanted));

    bool halted = false;
    QString haltText;

    for (qsizetype position = 0; position < wanted; ++position) {
        RowEmission emission;
        emission.sourceIndex = run.sourceIndexes.at(position);

        if (halted) {
            // Contagious: everything after the halting row was never attempted,
            // so its own (absent) error says nothing — the halt does.
            emission.message = haltText;
        } else if (position >= static_cast<qsizetype>(rows.size())) {
            emission.message = unattemptedText;
        } else {
            BatchSignRow& row = rows[static_cast<std::size_t>(position)];
            if (batchRowSucceeded(row.error, artifactSize(row.artifact))) {
                emission.ok = true;
                emission.artifact = std::move(row.artifact);
            } else {
                emission.message = errorText(row.error, CallError::None, {}, {});
                if (isHaltingError(row.error)) {
                    // Inclusive: the halting row is itself a failure, and it is
                    // where the halt text comes from.
                    outcome.haltRemainingRuns = true;
                    halted = true;
                    haltText = emission.message;
                }
            }
        }
        outcome.emissions.push_back(std::move(emission));
    }

    // Every descriptor this call was handed is now accounted for: a successful
    // row's moved into its emission, and every other one — including any extra
    // row a misbehaving agent answered beyond the run's own count — closed by
    // `rows` going out of scope right here.
    return outcome;
}

FdHandle openDocumentFd(const QString& filePath)
{
    // O_CLOEXEC: a document descriptor must never survive into a child process.
    // A failed open yields -1, which IS FdHandle's empty state — the caller
    // checks valid() and never has to interpret an errno.
    return FdHandle{::open(QFile::encodeName(filePath).constData(), O_RDONLY | O_CLOEXEC)};
}

QString outputPathFor(const SignRequestItem& item, const QString& outputFolder)
{
    const QFileInfo source(item.filePath);
    const QDir folder(outputFolder);

    QString path;
    switch (item.format) {
    case SignatureFormat::PAdES:
        path = folder.filePath(source.completeBaseName() + QStringLiteral("-signed.pdf"));
        break;
    case SignatureFormat::XAdES:
        // Detached XAdES keeps the full source name and adds its own suffix:
        // the signature is a SECOND file that must stay recognisable next to
        // the document it belongs to.
        path = item.packaging == Packaging::Enveloped
                   ? folder.filePath(source.completeBaseName() + QStringLiteral("-signed.xml"))
                   : folder.filePath(source.fileName() + QStringLiteral(".xsig"));
        break;
    case SignatureFormat::ASiCe:
        path = folder.filePath(source.completeBaseName() + QStringLiteral(".asice"));
        break;
    case SignatureFormat::CAdES:
        path = folder.filePath(source.fileName() + QStringLiteral(".p7s"));
        break;
    case SignatureFormat::JAdES:
        path = folder.filePath(source.fileName() + QStringLiteral(".jose"));
        break;
    }
    if (path.isEmpty() || !QFile::exists(path)) {
        return path;
    }

    // Never overwrite: today's numbered-sibling search, with today's cap. A
    // user who somehow has 10 000 numbered siblings gets an empty path, and
    // the caller's open-for-write failure surfaces a normal error rather than
    // this loop spinning forever.
    const QFileInfo taken(path);
    const QString base = taken.completeBaseName();
    const QString suffix = taken.suffix();
    const QString directory = taken.absolutePath();
    constexpr int kMaxSuffixAttempts = 10000;
    int counter = 2;
    do {
        path = QDir(directory).filePath(QStringLiteral("%1(%2).%3").arg(base).arg(counter).arg(suffix));
        ++counter;
    } while (QFile::exists(path) && counter <= kMaxSuffixAttempts);
    return QFile::exists(path) ? QString() : path;
}

QString artifactWriteFailedText()
{
    return qtTrId("lc-agent-error-artifact-write");
}

} // namespace librecelik::agent
