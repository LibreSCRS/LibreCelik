// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The ONE place the controller emission ordering is pinned.
///
/// Both the scripted fakes (`fake_gateway_selftest.cpp`) and the live
/// controllers (the live-agent suite) are asserted through these two helpers,
/// so a GUI suite can never pass on an ordering the real agent never produces.
///
/// Both helpers are ASYNC-CAPABLE BY CONSTRUCTION — the live leg's emissions
/// arrive through the event loop, the fakes' arrive synchronously inside the
/// verb call, and the same helper has to hold for both. Each one therefore:
///   1. installs its connections FIRST,
///   2. invokes the verb itself,
///   3. pumps the event loop via `waitFor` up to `expectation.waitMs`
///      (generous — a hardware leg needs tens of seconds) until the completion
///      predicate holds,
///   4. and only then asserts.
///
/// The expected script is OPTIONAL: the fake self-test passes the exact
/// scripted groups/rows and gets an equality assertion on top; the live leg
/// omits it and gets the ordering/count invariants alone.
///
/// Failures are reported with gtest's non-fatal `ADD_FAILURE`/`EXPECT_*` so a
/// broken ordering surfaces every violated clause in one run, not just the
/// first.

#pragma once

#include "agent/cardcontroller.h"
#include "agent/signcontroller.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QThread>

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <utility>

namespace librecelik::test::agent {

/// @brief Pump the event loop until @p pred holds or @p timeoutMs elapses.
///        Returns the predicate's final value, so a caller can assert on it.
template <typename Pred>
inline bool waitFor(Pred pred, int timeoutMs)
{
    QDeadlineTimer deadline(timeoutMs);
    while (!pred() && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return pred();
}

/// @brief What `assertReadEmissionContract` should additionally check.
struct ReadContractExpectation
{
    /// Upper bound on how long the read may take before the helper gives up.
    /// Generous on purpose — a live card read behind a human prompt is slow.
    int waitMs = 30000;
    /// The group keys the read is expected to stream, in order, INCLUDING the
    /// trailing photo group when one is scripted. `nullopt` (the live leg)
    /// asserts the ordering/count invariants only.
    std::optional<QStringList> expectedGroupKeys;
    /// True when the read is scripted to end in a photo group — the helper
    /// then requires the FINAL groupReady to carry the "photo" key and
    /// identityReady to contain it.
    bool expectPhotoGroup = false;
    /// True when the read is expected to fail; the helper then requires an
    /// errorOccurred and NO identityReady.
    bool expectError = false;
};

/// @brief What `assertSignEmissionContract` should additionally check.
struct SignContractExpectation
{
    /// Upper bound on how long the run may take before the helper gives up.
    int waitMs = 30000;
    /// Expected `(succeeded, failed)` tally. `nullopt` (the live leg) checks
    /// only that `finished` agrees with the rows the run itself emitted.
    std::optional<QPair<int, int>> expectedTally;
};

/// @brief Assert the READ emission contract on @p controller.
///
/// `readingStarted` -> zero-or-more `groupReady` -> (when the script carries a
/// photo) a FINAL `groupReady` whose group key is "photo", which
/// `identityReady` then contains -> `identityReady` -> `readingFinished`; and
/// `errorOccurred` implies NO `identityReady`.
inline void assertReadEmissionContract(librecelik::agent::CardController& controller,
                                       const ReadContractExpectation& expectation = {})
{
    QStringList order;        // the emission order, one token per emission
    QStringList groupKeys;    // groupReady keys, in order
    QStringList identityKeys; // the keys identityReady carried
    int identityCount = 0;
    int startedCount = 0;
    int finishedCount = 0;
    int errorCount = 0;

    // 1. connections FIRST — before the verb, so a synchronous fake is caught.
    const QMetaObject::Connection startedConn =
        QObject::connect(&controller, &librecelik::agent::CardController::readingStarted, [&] {
            ++startedCount;
            order << QStringLiteral("started");
        });
    const QMetaObject::Connection groupConn =
        QObject::connect(&controller, &librecelik::agent::CardController::groupReady,
                         [&](const LibreSCRS::AgentClient::FieldGroup& group) {
                             groupKeys << group.key;
                             order << QStringLiteral("group");
                         });
    const QMetaObject::Connection identityConn =
        QObject::connect(&controller, &librecelik::agent::CardController::identityReady,
                         [&](const QList<LibreSCRS::AgentClient::FieldGroup>& groups) {
                             ++identityCount;
                             identityKeys.clear();
                             for (const LibreSCRS::AgentClient::FieldGroup& group : groups) {
                                 identityKeys << group.key;
                             }
                             order << QStringLiteral("identity");
                         });
    const QMetaObject::Connection errorConn =
        QObject::connect(&controller, &librecelik::agent::CardController::errorOccurred, [&](const QString&) {
            ++errorCount;
            order << QStringLiteral("error");
        });
    const QMetaObject::Connection finishedConn =
        QObject::connect(&controller, &librecelik::agent::CardController::readingFinished, [&] {
            ++finishedCount;
            order << QStringLiteral("finished");
        });

    // 2. the verb itself, 3. pump until the completion predicate holds.
    controller.startRead();
    const bool completed = waitFor([&] { return finishedCount > 0 || errorCount > 0; }, expectation.waitMs);

    QObject::disconnect(startedConn);
    QObject::disconnect(groupConn);
    QObject::disconnect(identityConn);
    QObject::disconnect(errorConn);
    QObject::disconnect(finishedConn);

    // 4. assertions.
    if (!completed) {
        ADD_FAILURE() << "read never completed within " << expectation.waitMs
                      << " ms; emissions so far: " << qPrintable(order.join(QLatin1Char(',')));
        return;
    }

    EXPECT_EQ(startedCount, 1) << "readingStarted must fire exactly once";
    if (!order.isEmpty()) {
        EXPECT_EQ(order.first(), QStringLiteral("started")) << "readingStarted must be the FIRST emission";
    }

    if (expectation.expectError) {
        EXPECT_GT(errorCount, 0) << "a failing read must surface errorOccurred";
    }
    if (errorCount > 0) {
        EXPECT_EQ(identityCount, 0) << "errorOccurred must not be paired with identityReady";
        return;
    }

    EXPECT_EQ(identityCount, 1) << "identityReady must fire exactly once on a successful read";
    EXPECT_EQ(finishedCount, 1) << "readingFinished must fire exactly once";

    const int identityAt = order.indexOf(QStringLiteral("identity"));
    const int finishedAt = order.indexOf(QStringLiteral("finished"));
    const int lastGroupAt = order.lastIndexOf(QStringLiteral("group"));
    if (identityAt >= 0 && finishedAt >= 0) {
        EXPECT_LT(identityAt, finishedAt) << "identityReady must precede readingFinished";
    }
    if (lastGroupAt >= 0 && identityAt >= 0) {
        EXPECT_LT(lastGroupAt, identityAt) << "every groupReady must precede identityReady";
    }

    if (expectation.expectPhotoGroup) {
        ASSERT_FALSE(groupKeys.isEmpty()) << "a photo-carrying read must stream at least the photo group";
        EXPECT_EQ(groupKeys.last(), QStringLiteral("photo")) << "the photo group must be the FINAL groupReady";
        EXPECT_TRUE(identityKeys.contains(QStringLiteral("photo")))
            << "identityReady must carry the merged photo group";
    }

    if (expectation.expectedGroupKeys.has_value()) {
        EXPECT_EQ(groupKeys, *expectation.expectedGroupKeys) << "streamed group keys differ from the script";
        EXPECT_EQ(identityKeys, *expectation.expectedGroupKeys)
            << "identityReady's model differs from the streamed groups";
    }
}

/// @brief Assert the SIGN emission contract on @p controller.
///
/// `rowFinished` fires EXACTLY once per requested row (every source index of
/// the request exactly once, no invented indexes), ALL `rowFinished` emissions
/// precede `finished`, any `phaseChanged` arrives before `finished`, and
/// `finished` fires exactly once with a `(succeeded, failed)` pair equal to the
/// per-row `ok` tally.
inline void assertSignEmissionContract(librecelik::agent::SignController& controller, const QString& certId,
                                       const QList<librecelik::agent::SignRequestItem>& files,
                                       const LibreSCRS::AgentClient::SignOptions& options, const QString& outputFolder,
                                       const SignContractExpectation& expectation = {})
{
    QList<int> rowIndexes;
    int okRows = 0;
    int failedRows = 0;
    int phaseCount = 0;
    int finishedCount = 0;
    int succeeded = 0;
    int failed = 0;
    bool rowAfterFinished = false;
    bool phaseAfterFinished = false;

    // 1. connections FIRST.
    const QMetaObject::Connection phaseConn =
        QObject::connect(&controller, &librecelik::agent::SignController::phaseChanged,
                         [&](LibreSCRS::AgentClient::OperationPhase, double) {
                             ++phaseCount;
                             if (finishedCount > 0) {
                                 phaseAfterFinished = true;
                             }
                         });
    const QMetaObject::Connection rowConn =
        QObject::connect(&controller, &librecelik::agent::SignController::rowFinished,
                         [&](int index, const librecelik::agent::SignRowResult& result) {
                             rowIndexes << index;
                             if (result.ok) {
                                 ++okRows;
                             } else {
                                 ++failedRows;
                             }
                             if (finishedCount > 0) {
                                 rowAfterFinished = true;
                             }
                         });
    const QMetaObject::Connection finishedConn =
        QObject::connect(&controller, &librecelik::agent::SignController::finished, [&](int ok, int bad) {
            ++finishedCount;
            succeeded = ok;
            failed = bad;
        });

    // 2. the verb itself, 3. pump until the completion predicate holds.
    controller.start(certId, files, options, outputFolder);
    const bool completed = waitFor([&] { return finishedCount > 0; }, expectation.waitMs);

    QObject::disconnect(phaseConn);
    QObject::disconnect(rowConn);
    QObject::disconnect(finishedConn);

    // 4. assertions.
    if (!completed) {
        ADD_FAILURE() << "sign run never finished within " << expectation.waitMs
                      << " ms; rows seen: " << rowIndexes.size();
        return;
    }

    EXPECT_EQ(finishedCount, 1) << "finished must fire exactly once";
    EXPECT_FALSE(rowAfterFinished) << "every rowFinished must precede finished";
    EXPECT_FALSE(phaseAfterFinished) << "every phaseChanged must precede finished";
    EXPECT_EQ(rowIndexes.size(), files.size()) << "rowFinished must fire exactly once per requested row";

    QList<int> sortedIndexes = rowIndexes;
    std::sort(sortedIndexes.begin(), sortedIndexes.end());
    QList<int> expectedIndexes;
    expectedIndexes.reserve(files.size());
    for (int index = 0; index < files.size(); ++index) {
        expectedIndexes << index;
    }
    EXPECT_EQ(sortedIndexes, expectedIndexes) << "every source index exactly once, no invented indexes";

    EXPECT_EQ(succeeded, okRows) << "finished's succeeded count must equal the per-row ok tally";
    EXPECT_EQ(failed, failedRows) << "finished's failed count must equal the per-row failure tally";

    if (expectation.expectedTally.has_value()) {
        EXPECT_EQ(qMakePair(succeeded, failed), *expectation.expectedTally) << "tally differs from the script";
    }
    // phaseCount is unconstrained by the contract (an agent may report none);
    // it is recorded so the before-finished ordering above can be judged.
    Q_UNUSED(phaseCount)
}

} // namespace librecelik::test::agent
