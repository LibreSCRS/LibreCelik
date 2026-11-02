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
#include "agent/agentbootstrap.h"

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

    /// @brief Render @p state, the gateway's presence verdict.
    void setState(librecelik::agent::PresenceState state);

    /// @brief Where the missing-agent state points the reader for installing
    ///        the agent, in the window's current language.
    void setInstallHelpUrl(const QString& url);

    /// @brief Replace the missing-agent copy with what is being done about it.
    ///
    /// Only the `AgentMissing` state speaks through it; `None` restores the
    /// panel's own "not installed" copy. Kept across state changes, so an
    /// agent that comes and goes again finds the guidance it left with.
    void setGuidance(librecelik::agent::Guidance guidance);

    /// @brief Hold the retry affordance while a start is being answered.
    void setStarting(bool starting);

signals:
    /// @brief The retry affordance was used. The window answers it by asking
    ///        for the agent to be started (`AgentBootstrap::retry()`); the
    ///        panel holds no gateway of its own (its constructor takes a
    ///        parent and nothing else).
    void retryRequested();

protected:
    void changeEvent(QEvent* event) override; // retranslate

private:
    void retranslateUi();

    PresenceState presenceState = PresenceState::Ready;
    Guidance guidance = Guidance::None;

    QLabel* titleLabel = nullptr;
    QLabel* hintLabel = nullptr;
    QLabel* installHelpLabel = nullptr;
    QString installHelpUrl;
    QPushButton* retryButton = nullptr;
};

} // namespace librecelik::agent
