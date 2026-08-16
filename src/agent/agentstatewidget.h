// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The guided-UX empty-state panel: what the main window shows when
///        there is no agent to read cards with.
///
/// LC does no card I/O of its own, so "nothing to show" has three causes and
/// exactly one of them is the ordinary one. The panel names the two that are
/// not: an agent that was never installed (nothing LC can do but say so and
/// point at the fix) and an agent that is installed but not answering (which a
/// retry can genuinely resolve, so it gets one). The ordinary case — an agent
/// that is there and simply has no card yet — is the window's own "insert a
/// card" copy, and this panel stays silent for it rather than saying the same
/// thing twice.

#pragma once

#include "agent/agentgateway.h"

#include <QWidget>

class QEvent;
class QLabel;
class QPushButton;

namespace librecelik::agent {

class AgentStateWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AgentStateWidget(QWidget* parent = nullptr);

    /// @brief Render @p state.
    /// @param state     The gateway's presence verdict.
    /// @param anyReader Whether the agent currently enumerates any reader.
    ///                  Recorded for the guided-state trace (the manual smoke
    ///                  reads it) — both `Ready` shapes render the window's own
    ///                  copy, so it changes no pixel here.
    void setState(librecelik::agent::PresenceState state, bool anyReader);

signals:
    /// @brief The retry affordance was used. The window answers it with
    ///        `AgentGateway::refresh()`; the panel holds no gateway of its own
    ///        (its constructor takes a parent and nothing else).
    void retryRequested();

protected:
    void changeEvent(QEvent* event) override; // retranslate

private:
    void retranslateUi();

    PresenceState presenceState = PresenceState::Ready;
    bool readerPresent = false;

    QLabel* titleLabel = nullptr;
    QLabel* hintLabel = nullptr;
    QPushButton* retryButton = nullptr;
};

} // namespace librecelik::agent
