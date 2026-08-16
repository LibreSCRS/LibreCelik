// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "fake_gateway/fakesigncontroller.h"

#include "fake_gateway/fakeagentgateway.h"

#include <utility>

namespace librecelik::test::agent {

FakeSignController::FakeSignController(QObject* parent) : librecelik::agent::SignController(parent)
{
    registerFakeMetatypes();
}

bool FakeSignController::canVisualSign() const
{
    return scriptedCanVisual;
}

bool FakeSignController::canTsaOverride() const
{
    return scriptedCanTsa;
}

bool FakeSignController::canBatch() const
{
    return scriptedCanBatch;
}

void FakeSignController::start(const QString& certId, const QList<librecelik::agent::SignRequestItem>& files,
                               const LibreSCRS::AgentClient::SignOptions& options, const QString& outputFolder)
{
    callLog << QStringLiteral("start");
    lastCertId = certId;
    lastFiles = files;
    lastOptions = options;
    lastOutputFolder = outputFolder;
    if (holdOpen) {
        return;
    }
    replay();
}

void FakeSignController::cancel()
{
    callLog << QStringLiteral("cancel");
    cancelCalled = true;
}

void FakeSignController::releaseHold()
{
    holdOpen = false;
    replay();
}

void FakeSignController::replay()
{
    for (const LibreSCRS::AgentClient::OperationPhase phase : std::as_const(scriptedPhases)) {
        Q_EMIT phaseChanged(phase, 0.0);
    }
    int succeeded = 0;
    int failed = 0;
    for (int index = 0; index < scriptedRows.size(); ++index) {
        const librecelik::agent::SignRowResult& row = scriptedRows.at(index);
        if (row.ok) {
            ++succeeded;
        } else {
            ++failed;
        }
        Q_EMIT rowFinished(index, row);
    }
    Q_EMIT finished(succeeded, failed);
}

} // namespace librecelik::test::agent
