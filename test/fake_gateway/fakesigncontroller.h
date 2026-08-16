// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Scripted `SignController` for offscreen GUI tests.
///
/// Test-only scaffolding: compiled into test targets, never shipped.
/// `start()` records the request and replays the scripted phases and rows in
/// the SAME order the live controller guarantees — every scripted row before
/// `finished`, whose `(succeeded, failed)` tally is derived from the rows,
/// never scripted independently. The ordering itself is asserted through
/// `controllercontract.h`, which both this fake and the live controller are
/// pinned against.

#pragma once

#include "agent/signcontroller.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace librecelik::test::agent {

class FakeSignController : public librecelik::agent::SignController
{
    Q_OBJECT
public:
    explicit FakeSignController(QObject* parent = nullptr);

    // ---- scripted state -----------------------------------------------------
    /// Replayed one `rowFinished` per entry, in order, before `finished`.
    QList<librecelik::agent::SignRowResult> scriptedRows;
    /// Replayed as `phaseChanged` emissions from `start()` itself, before the
    /// rows — including when the terminal is held (see @ref holdOpen).
    QList<LibreSCRS::AgentClient::OperationPhase> scriptedPhases;
    bool scriptedCanVisual = true;
    bool scriptedCanTsa = true;
    bool scriptedCanBatch = true;
    /// Engaged script => `start()` records the request, reports the scripted
    /// phases, and then emits nothing more until `releaseHold()` replays the
    /// rows and the terminal. That is the in-flight shape a cancel test needs,
    /// and also the shape of a run parked at the agent's prompter: it has
    /// already said where it is, and what it did is still unknown.
    bool holdOpen = false;
    bool cancelCalled = false;
    /// Records verb calls for assertions.
    QStringList callLog;
    // ---- last recorded request ----------------------------------------------
    QString lastCertId;
    QList<librecelik::agent::SignRequestItem> lastFiles;
    LibreSCRS::AgentClient::SignOptions lastOptions;
    QString lastOutputFolder;

    /// Replay the scripted emissions a `holdOpen` `start()` withheld.
    void releaseHold();

    // ---- SignController ------------------------------------------------------
    [[nodiscard]] bool canVisualSign() const override;
    [[nodiscard]] bool canTsaOverride() const override;
    [[nodiscard]] bool canBatch() const override;

    void start(const QString& certId, const QList<librecelik::agent::SignRequestItem>& files,
               const LibreSCRS::AgentClient::SignOptions& options, const QString& outputFolder) override;
    void cancel() override;

private:
    void replay();
};

} // namespace librecelik::test::agent
