// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// The sign path's agent-less decisions, and the one place the batch wire's
// observed behaviour is written down as executable rules rather than as
// handler prose: rows carry NO status field (success is derived), a failed row
// still resolves a REAL zero-length descriptor that must be drained, rows
// arrive on a Finished(Error) terminal too, a wrong or blocked credential
// halts inclusively and contagiously, and a rate limit is surfaced retryable —
// never retried. Every case runs without an agent, which is the point: the
// consumption logic is provable in CI, not only on a bench.

#include "agent/signrequest.h"

#include "agent/errortext.h"

#include <LibreSCRS/AgentClient/ErrorCode.h>
#include <LibreSCRS/AgentClient/FdHandle.h>
#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <LibreSCRS/AgentClient/SignOptions.h>

#include <QByteArray>
#include <QList>
#include <QRectF>
#include <QString>
#include <QtGlobal>

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#ifdef Q_OS_LINUX
#include <dirent.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace LibreSCRS::AgentClient;
using namespace librecelik::agent;

TEST(SignRequest, LevelTokensMapAndUnknownIsAuto)
{
    EXPECT_EQ(levelFromUiToken(QStringLiteral("B_B")), SignatureLevel::BB);
    EXPECT_EQ(levelFromUiToken(QStringLiteral("B_T")), SignatureLevel::BT);
    EXPECT_EQ(levelFromUiToken(QStringLiteral("B_LT")), SignatureLevel::BLT);
    EXPECT_EQ(levelFromUiToken(QStringLiteral("B_LTA")), SignatureLevel::BLTA);
    EXPECT_EQ(levelFromUiToken({}), SignatureLevel::Auto);
}

TEST(SignRequest, VisualMapCarriesAllSixRequiredKeys)
{
    const auto m = makeVisualSignatureMap(0, QRectF(10, 20, 200, 50), QStringLiteral("t"));
    for (const char* k : {"page", "x", "y", "width", "height", "text"})
        EXPECT_TRUE(m.contains(QLatin1String(k))) << k;
    EXPECT_EQ(m.value(QStringLiteral("page")).toUInt(), 0u);
}

TEST(SignRequest, PartitionIsStableSortedNotConsecutive)
{
    SignRequestItem pdf{QStringLiteral("a.pdf"), SignatureFormat::PAdES, Packaging::Enveloped};
    SignRequestItem xml{QStringLiteral("b.xml"), SignatureFormat::XAdES, Packaging::Enveloped};
    SignRequestItem pdf2{QStringLiteral("c.pdf"), SignatureFormat::PAdES, Packaging::Enveloped};
    // Interleaved kinds still make exactly TWO runs (two consents, not three).
    const auto runs = partitionIntoRuns({pdf, xml, pdf2});
    ASSERT_EQ(runs.size(), 2);
    EXPECT_EQ(runs[0].sourceIndexes, (QList<int>{0, 2})); // selection order kept inside the run
    EXPECT_EQ(runs[0].items[1].filePath, QStringLiteral("c.pdf"));
    EXPECT_EQ(runs[1].sourceIndexes, (QList<int>{1}));
}

// The per-run dial. What a run's options carry is decided here rather than in
// the live controller's dispatch loop, so the decision is provable without an
// agent: the fds a real dispatch moves into the call make the options the only
// thing left to read afterwards.
TEST(SignRequest, RunOptionsTakeTheKindFromTheRunNotTheRequest)
{
    SignOptions requested;
    requested.format = SignatureFormat::PAdES; // whatever the wizard last set
    requested.packaging = Packaging::Enveloped;
    const SignRun run{{1},
                      {SignRequestItem{QStringLiteral("/tmp/data.xml"), SignatureFormat::XAdES, Packaging::Detached}}};
    const SignOptions dialled = runSignOptions(requested, run);
    EXPECT_EQ(dialled.format, SignatureFormat::XAdES);
    EXPECT_EQ(dialled.packaging, Packaging::Detached);
}

TEST(SignRequest, SingleRunOptionsCarryTheDocumentName)
{
    // NOT chrome: the ASiC-E data entry is named from this and the detached
    // JAdES/XAdES reference URI derives from it, so a byte-path single sign
    // dialled without it is refused agent-side.
    const SignRun run{
        {0}, {SignRequestItem{QStringLiteral("/tmp/dir/report.docx"), SignatureFormat::ASiCe, Packaging::Enveloped}}};
    EXPECT_EQ(runSignOptions({}, run).displayName, QStringLiteral("report.docx"));
}

TEST(SignRequest, BatchRunOptionsCarryNoDocumentName)
{
    // A batch names each document on its own row, and the agent never reads a
    // displayName option off a SignBatch request — one file's name riding a
    // whole batch's options would name the wrong documents.
    const SignRun run{{0, 1},
                      {SignRequestItem{QStringLiteral("/tmp/a.pdf"), SignatureFormat::PAdES, Packaging::Enveloped},
                       SignRequestItem{QStringLiteral("/tmp/b.pdf"), SignatureFormat::PAdES, Packaging::Enveloped}}};
    EXPECT_TRUE(runSignOptions({}, run).displayName.isEmpty());
}

TEST(SignRequest, DisplayNameIsTheFileNameNeverThePath)
{
    EXPECT_EQ(
        documentDisplayName({QStringLiteral("/tmp/dir/report.docx"), SignatureFormat::ASiCe, Packaging::Enveloped}),
        QStringLiteral("report.docx"));
}

// The output-naming contract, per format and packaging. These cases came from
// the signing suite's local copy of the sign page's path builder; they call the
// production helper now, so the names a user finds in their output folder are
// pinned by the code that actually produces them. The folder does not exist,
// which is deliberate: the never-overwrite search only engages for a path that
// is already taken.
TEST(SignRequest, OutputNamePAdESIsBaseNameSigned)
{
    const SignRequestItem item{QStringLiteral("/tmp/document.pdf"), SignatureFormat::PAdES, Packaging::Enveloped};
    EXPECT_EQ(outputPathFor(item, QStringLiteral("/out")), QStringLiteral("/out/document-signed.pdf"));
}

TEST(SignRequest, OutputNameXAdESEnvelopedIsBaseNameSigned)
{
    const SignRequestItem item{QStringLiteral("/tmp/data.xml"), SignatureFormat::XAdES, Packaging::Enveloped};
    EXPECT_EQ(outputPathFor(item, QStringLiteral("/out")), QStringLiteral("/out/data-signed.xml"));
}

TEST(SignRequest, OutputNameXAdESDetachedKeepsTheWholeSourceName)
{
    // The detached signature is a SECOND file: it stays recognisable next to
    // the document it belongs to, extension included.
    const SignRequestItem item{QStringLiteral("/tmp/data.xml"), SignatureFormat::XAdES, Packaging::Detached};
    EXPECT_EQ(outputPathFor(item, QStringLiteral("/out")), QStringLiteral("/out/data.xml.xsig"));
}

TEST(SignRequest, OutputNameAsicEIsBaseNameContainer)
{
    const SignRequestItem item{QStringLiteral("/tmp/report.docx"), SignatureFormat::ASiCe, Packaging::Enveloped};
    EXPECT_EQ(outputPathFor(item, QStringLiteral("/out")), QStringLiteral("/out/report.asice"));
}

TEST(SignRequest, OutputNameCAdESKeepsTheWholeSourceName)
{
    const SignRequestItem item{QStringLiteral("/tmp/file.bin"), SignatureFormat::CAdES, Packaging::Detached};
    EXPECT_EQ(outputPathFor(item, QStringLiteral("/out")), QStringLiteral("/out/file.bin.p7s"));
}

TEST(SignRequest, OutputNameJAdESKeepsTheWholeSourceName)
{
    const SignRequestItem item{QStringLiteral("/tmp/data.json"), SignatureFormat::JAdES, Packaging::Detached};
    EXPECT_EQ(outputPathFor(item, QStringLiteral("/out")), QStringLiteral("/out/data.json.jose"));
}

TEST(SignRequest, OutputNameStripsOnlyTheLastExtension)
{
    const SignRequestItem item{QStringLiteral("/tmp/archive.tar.gz"), SignatureFormat::ASiCe, Packaging::Enveloped};
    EXPECT_EQ(outputPathFor(item, QStringLiteral("/out")), QStringLiteral("/out/archive.tar.asice"));
}

TEST(SignRequest, RowSuccessNeedsNoErrorAndNonEmptyArtifact)
{
    EXPECT_TRUE(batchRowSucceeded(ErrorCode::None, 2128));
    EXPECT_FALSE(batchRowSucceeded(ErrorCode::None, 0)); // zero-length != success
    EXPECT_FALSE(batchRowSucceeded(ErrorCode::CredentialWrong, 0));
}

#ifdef Q_OS_LINUX
namespace {

/// The process's open-descriptor count, measured the SAME way in every census:
/// one directory handle over /proc/self/fd, whose own descriptor and the two
/// dot entries are a constant offset the before/after comparison cancels.
[[nodiscard]] int openFdCensus()
{
    DIR* dir = ::opendir("/proc/self/fd");
    if (dir == nullptr) {
        return -1;
    }
    int count = 0;
    while (::readdir(dir) != nullptr) {
        ++count;
    }
    ::closedir(dir);
    return count;
}

/// One row-sized descriptor of exactly the shape the wire hands over: an
/// anonymous file carrying @p bytes bytes — ZERO for a failed row, which the
/// wire still answers with a real, open, zero-length handle rather than an
/// invalid one.
[[nodiscard]] FdHandle makeCountedFd(int bytes)
{
    const int fd = memfd_create("row", 0);
    if (fd < 0) {
        return FdHandle{};
    }
    if (bytes > 0) {
        const QByteArray payload(bytes, 'x');
        if (::write(fd, payload.constData(), static_cast<std::size_t>(payload.size())) != payload.size()) {
            ::close(fd);
            return FdHandle{};
        }
    }
    return FdHandle{fd};
}

[[nodiscard]] BatchSignRow makeRow(int bytes, ErrorCode error)
{
    BatchSignRow row;
    row.displayName = QStringLiteral("d.pdf");
    row.artifact = makeCountedFd(bytes);
    row.error = error;
    return row;
}

} // namespace
#endif

namespace {

/// A run of @p indexes selection-order rows, all of one kind (the shape
/// `partitionIntoRuns` produces).
[[nodiscard]] SignRun runOf(const QList<int>& indexes)
{
    SignRun run;
    run.sourceIndexes = indexes;
    for (int i = 0; i < indexes.size(); ++i) {
        run.items.append(SignRequestItem{QStringLiteral("d.pdf"), SignatureFormat::PAdES, Packaging::Enveloped});
    }
    return run;
}

} // namespace

TEST(ConsumeBatchOutcome, ResultRowsConsumedEvenOnFinishedError)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "the anonymous-file rows are the Linux wire's shape; the consumption logic is platform-neutral";
#else
    // The operation itself finished Error, and the rows STILL arrive — a row
    // that named no error and carries bytes was signed regardless of what the
    // operation as a whole reported.
    std::vector<BatchSignRow> rows;
    rows.push_back(makeRow(12, ErrorCode::None));
    rows.push_back(makeRow(0, ErrorCode::SigningEngineError));
    const BatchOutcome outcome =
        consumeBatchOutcome(OperationStatus::Error, ErrorCode::SigningEngineError, std::move(rows), runOf({0, 1}));
    ASSERT_EQ(outcome.emissions.size(), 2u); // never fewer than the run's rows
    EXPECT_TRUE(outcome.emissions[0].ok);
    EXPECT_TRUE(outcome.emissions[0].artifact.valid());
    EXPECT_TRUE(outcome.emissions[0].message.isEmpty());
    EXPECT_FALSE(outcome.emissions[1].ok);
    EXPECT_FALSE(outcome.emissions[1].artifact.valid()); // a failed row hands nothing on to write
    EXPECT_EQ(outcome.emissions[1].message, errorText(ErrorCode::SigningEngineError, CallError::None, {}, {}));
#endif
}

TEST(ConsumeBatchOutcome, EveryRowFdDrainedIncludingFailedZeroLengthRows)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "the descriptor census is /proc/self/fd; the drain rule it proves is platform-neutral";
#else
    // Warm the message path before the census so a first-call allocation
    // cannot masquerade as a leaked descriptor.
    (void)errorText(ErrorCode::CredentialWrong, CallError::None, {}, {});
    const int before = openFdCensus();
    ASSERT_GT(before, 0);
    {
        std::vector<BatchSignRow> rows;
        rows.push_back(makeRow(9, ErrorCode::None));
        rows.push_back(makeRow(0, ErrorCode::CredentialWrong)); // real, open, zero-length
        rows.push_back(makeRow(0, ErrorCode::CredentialWrong));
        ASSERT_EQ(openFdCensus(), before + 3); // non-vacuous: the census can see them
        const BatchOutcome outcome =
            consumeBatchOutcome(OperationStatus::Error, ErrorCode::CredentialWrong, std::move(rows), runOf({0, 1, 2}));
        ASSERT_EQ(outcome.emissions.size(), 3u);
    }
    EXPECT_EQ(openFdCensus(), before); // every descriptor closed — succeeded and failed alike
#endif
}

TEST(ConsumeBatchOutcome, HaltIsInclusiveAndContagious)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "the anonymous-file rows are the Linux wire's shape; the halt rule is platform-neutral";
#else
    std::vector<BatchSignRow> rows;
    rows.push_back(makeRow(7, ErrorCode::None));
    rows.push_back(makeRow(0, ErrorCode::CredentialWrong)); // the halting row
    // Past the halt the agent stopped working: no error of its own, no bytes.
    rows.push_back(makeRow(0, ErrorCode::None));
    const BatchOutcome outcome =
        consumeBatchOutcome(OperationStatus::Error, ErrorCode::CredentialWrong, std::move(rows), runOf({0, 1, 2}));

    ASSERT_EQ(outcome.emissions.size(), 3u);
    EXPECT_TRUE(outcome.emissions[0].ok);
    EXPECT_FALSE(outcome.emissions[1].ok); // inclusive: the halting row failed too
    EXPECT_EQ(outcome.emissions[1].message, errorText(ErrorCode::CredentialWrong, CallError::None, {}, {}));
    // Contagious: the row after the halt reports the HALT, not the generic line
    // its own empty error would otherwise resolve to.
    EXPECT_FALSE(outcome.emissions[2].ok);
    EXPECT_EQ(outcome.emissions[2].message, outcome.emissions[1].message);
    EXPECT_NE(outcome.emissions[2].message, errorText(ErrorCode::None, CallError::None, {}, {}));
    EXPECT_TRUE(outcome.haltRemainingRuns);
#endif
}

TEST(ConsumeBatchOutcome, RateLimitedSurfacesRetryableAndHalts)
{
    // The rate limit lands on the OPERATION — between runs, before any row of
    // this one was attempted, so the wire answers no rows at all.
    const BatchOutcome outcome = consumeBatchOutcome(OperationStatus::Error, ErrorCode::RateLimited, {}, runOf({0, 1}));

    ASSERT_EQ(outcome.emissions.size(), 2u); // no file is silently dropped
    EXPECT_TRUE(outcome.retryableRateLimit);
    EXPECT_FALSE(outcome.emissions[0].ok);
    EXPECT_EQ(outcome.emissions[0].message, errorText(ErrorCode::RateLimited, CallError::None, {}, {}));
    EXPECT_NE(outcome.emissions[0].message, errorText(ErrorCode::None, CallError::None, {}, {}));
    EXPECT_EQ(outcome.emissions[1].message, outcome.emissions[0].message);
    // Stopping is what the retryable flag is FOR; the halt flag stays the
    // credential rule's own. Neither one is an auto-retry — no such field
    // exists, and the user re-runs when ready.
    EXPECT_FALSE(outcome.haltRemainingRuns);
}

TEST(ConsumeBatchOutcome, EmissionsMapThroughSourceIndexes)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "the anonymous-file rows are the Linux wire's shape; the index mapping is platform-neutral";
#else
    // A run's rows are ITS rows; the wizard displays selection-order rows, and
    // the interleaved selection that produced this run skipped index 1.
    std::vector<BatchSignRow> rows;
    rows.push_back(makeRow(3, ErrorCode::None));
    rows.push_back(makeRow(4, ErrorCode::None));
    const BatchOutcome outcome =
        consumeBatchOutcome(OperationStatus::Ok, ErrorCode::None, std::move(rows), runOf({0, 2}));

    ASSERT_EQ(outcome.emissions.size(), 2u);
    EXPECT_EQ(outcome.emissions[0].sourceIndex, 0);
    EXPECT_EQ(outcome.emissions[1].sourceIndex, 2);
#endif
}
