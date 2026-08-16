// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The production `SignController`: one card's signing flows over the
///        agent client, in the LC vocabulary the wizard consumes.
///
/// Three things it adds to what the client offers. The RUN structure: a
/// selection is partitioned into homogeneous (format, packaging) runs, and one
/// run is one call — so the consents a user pays for depend on what was
/// selected, never on the order it was selected in. The CALLER-SIDE stall
/// bound per operation (the library has none — see `OpStallWatchdog`). And the
/// WRITE: the agent hands back descriptors, and turning those into files in
/// the user's output folder, without ever losing one, is this class's job —
/// delegated row-by-row to the pure `consumeBatchOutcome`, which is where the
/// batch wire's observed behaviour is written down and tested.

#pragma once

#include "agent/signcontroller.h"
#include "agent/signrequest.h"

#include <LibreSCRS/AgentClient/FdHandle.h>
#include <LibreSCRS/AgentClient/SignOptions.h>

#include <QList>
#include <QPointer>
#include <QString>

namespace LibreSCRS::AgentClient {
class AgentCard;
class AgentClient;
class AgentOperation;
} // namespace LibreSCRS::AgentClient

namespace librecelik::agent {

class LiveSignController final : public SignController
{
    Q_OBJECT
public:
    LiveSignController(LibreSCRS::AgentClient::AgentCard* card, LibreSCRS::AgentClient::AgentClient* client,
                       QObject* parent = nullptr);
    ~LiveSignController() override;

    [[nodiscard]] bool canVisualSign() const override;
    [[nodiscard]] bool canTsaOverride() const override;
    [[nodiscard]] bool canBatch() const override;

    void start(const QString& certId, const QList<SignRequestItem>& files,
               const LibreSCRS::AgentClient::SignOptions& options, const QString& outputFolder) override;
    void cancel() override;

private:
    /// Dial runs until one is in flight, or none is left and the request ends.
    void dispatchNextRun();
    /// Turn one finished operation into written files and per-row results, and
    /// decide whether the remaining runs may still be dialled.
    void consumeFinished(LibreSCRS::AgentClient::AgentOperation* operation, bool watchdogFired, bool batched,
                         const SignRun& run);
    /// Stream one signed artifact into the output folder; the written path, or
    /// empty when the artifact could not be written.
    [[nodiscard]] QString writeArtifact(const SignRequestItem& item, LibreSCRS::AgentClient::FdHandle artifact);

    /// Report one selection-order row and keep the running tally.
    void emitRow(int sourceIndex, bool ok, const QString& outputPath, const QString& message);
    /// Report every row of @p run as failed with @p message.
    void failRun(const SignRun& run, const QString& message);
    /// Report every row of every run not yet dialled as failed, and dial no
    /// more of them.
    void failRemainingRuns(const QString& message);
    void finishRequest();

    /// The card is DELETED by the client on removal while queued slots may
    /// still run, so it is held weakly and null-checked at every use.
    QPointer<LibreSCRS::AgentClient::AgentCard> card;
    /// Owned by the process-wide shared client the gateway holds alive, and the
    /// gateway parents this controller, so it cannot dangle here. Used for the
    /// feature membership checks only.
    LibreSCRS::AgentClient::AgentClient* client = nullptr;
    /// The single in-flight operation, weakly held: one run is dialled at a
    /// time, because a second consent while the first is at the prompter is
    /// exactly the pile-up the run structure exists to prevent.
    QPointer<LibreSCRS::AgentClient::AgentOperation> inFlight;

    // ---- the request in progress -------------------------------------------
    QString certId;
    /// The SELECTION, in the order the wizard displays it — every row index
    /// this controller emits indexes into this list.
    QList<SignRequestItem> files;
    LibreSCRS::AgentClient::SignOptions options;
    QString outputFolder;
    QList<SignRun> runs;
    int nextRun = 0;
    int succeeded = 0;
    int failed = 0;
    bool running = false;
    /// A cancel was asked for. The in-flight operation still delivers its own
    /// terminal (a cancel is fire-and-forget), so this is read there — the runs
    /// behind it are reported with the same reason rather than vanishing from
    /// the result list.
    bool cancelRequested = false;
};

} // namespace librecelik::agent
