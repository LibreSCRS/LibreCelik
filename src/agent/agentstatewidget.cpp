// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/agentstatewidget.h"

#include "utils/libreceliklog.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace librecelik::agent {

AgentStateWidget::AgentStateWidget(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(6);

    titleLabel = new QLabel(this);
    // Test seam: the offscreen GUI suite reads the rendered per-state message
    // through this name. hintLabel carries only the AgentMissing install hint,
    // so the title is the one label every guided state speaks through.
    titleLabel->setObjectName(QStringLiteral("agentStateMessage"));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet(QStringLiteral("QLabel { font-size: 14px; font-weight: bold; }"));
    layout->addWidget(titleLabel);

    hintLabel = new QLabel(this);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    retryButton = new QPushButton(this);
    layout->addWidget(retryButton, 0, Qt::AlignHCenter);
    connect(retryButton, &QPushButton::clicked, this, &AgentStateWidget::retryRequested);

    // Nothing is known about the agent until the window says so; the panel
    // starts in the state that renders nothing.
    retranslateUi();
}

void AgentStateWidget::setState(librecelik::agent::PresenceState state, bool anyReader)
{
    presenceState = state;
    readerPresent = anyReader;
    // The guided states are exactly what a manual smoke has to be able to see
    // in the log — a blank panel and a panel that was never asked to render
    // look identical on screen.
    qCInfo(lcGeneral) << "AgentStateWidget: presence="
                      << (state == PresenceState::Ready          ? "Ready"
                          : state == PresenceState::AgentMissing ? "AgentMissing"
                                                                 : "AgentUnavailable")
                      << "anyReader=" << anyReader;
    retranslateUi();
}

void AgentStateWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AgentStateWidget::retranslateUi()
{
    switch (presenceState) {
    case PresenceState::AgentMissing:
        titleLabel->setText(qtTrId("lc-agent-state-missing"));
        hintLabel->setText(qtTrId("lc-agent-state-missing-hint"));
        break;
    case PresenceState::AgentUnavailable:
        titleLabel->setText(qtTrId("lc-agent-state-unavailable"));
        hintLabel->clear();
        break;
    case PresenceState::Ready:
        titleLabel->clear();
        hintLabel->clear();
        break;
    }
    retryButton->setText(qtTrId("lc-agent-state-retry"));

    const bool guided = presenceState != PresenceState::Ready;
    titleLabel->setVisible(guided);
    hintLabel->setVisible(guided && !hintLabel->text().isEmpty());
    // Only the unavailable state is retryable: a missing agent does not start
    // because LC asked it to, and an offered button that cannot work is worse
    // than no button.
    retryButton->setVisible(presenceState == PresenceState::AgentUnavailable);
    setVisible(guided);
}

} // namespace librecelik::agent
