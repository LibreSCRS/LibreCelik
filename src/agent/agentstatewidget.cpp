// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/agentstatewidget.h"

#include "utils/libreceliklog.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace librecelik::agent {

namespace {

const char* guidanceName(Guidance guidance)
{
    switch (guidance) {
    case Guidance::None:
        return "None";
    case Guidance::Starting:
        return "Starting";
    case Guidance::NotInstalled:
        return "NotInstalled";
    case Guidance::NotInApplications:
        return "NotInApplications";
    case Guidance::StillMissing:
        return "StillMissing";
    }
    return "?";
}

/// The title a guidance replaces the missing-agent copy with. Read on every
/// retranslate, never cached: a language switch must reach it at once.
QString guidanceTitle(Guidance guidance)
{
    switch (guidance) {
    case Guidance::Starting:
        return qtTrId("lc-agent-guidance-starting");
    case Guidance::NotInstalled:
        return qtTrId("lc-agent-guidance-not-installed");
    case Guidance::NotInApplications:
        return qtTrId("lc-agent-guidance-not-in-applications");
    case Guidance::StillMissing:
        return qtTrId("lc-agent-guidance-still-missing");
    case Guidance::None:
        break;
    }
    return {};
}

} // namespace

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

    installHelpLabel = new QLabel(this);
    installHelpLabel->setObjectName(QStringLiteral("agentInstallHelp"));
    installHelpLabel->setAlignment(Qt::AlignCenter);
    installHelpLabel->setTextFormat(Qt::RichText);
    installHelpLabel->setOpenExternalLinks(true);
    layout->addWidget(installHelpLabel);

    retryButton = new QPushButton(this);
    layout->addWidget(retryButton, 0, Qt::AlignHCenter);
    connect(retryButton, &QPushButton::clicked, this, &AgentStateWidget::retryRequested);

    // Nothing is known about the agent until the window says so; the panel
    // starts in the state that renders nothing.
    retranslateUi();
}

void AgentStateWidget::setState(librecelik::agent::PresenceState state)
{
    presenceState = state;
    // The guided states are exactly what a manual smoke has to be able to see
    // in the log — a blank panel and a panel that was never asked to render
    // look identical on screen.
    qCInfo(lcGeneral) << "AgentStateWidget: presence="
                      << (state == PresenceState::Ready          ? "Ready"
                          : state == PresenceState::AgentMissing ? "AgentMissing"
                                                                 : "AgentUnavailable")
                      << "guidance=" << guidanceName(guidance);
    retranslateUi();
}

void AgentStateWidget::setInstallHelpUrl(const QString& url)
{
    installHelpUrl = url;
    retranslateUi();
}

void AgentStateWidget::setGuidance(librecelik::agent::Guidance value)
{
    guidance = value;
    qCInfo(lcGeneral) << "AgentStateWidget: guidance=" << guidanceName(guidance);
    retranslateUi();
}

void AgentStateWidget::setStarting(bool starting)
{
    retryButton->setEnabled(!starting);
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
        if (guidance == Guidance::None) {
            titleLabel->setText(qtTrId("lc-agent-state-missing"));
            hintLabel->setText(qtTrId("lc-agent-state-missing-hint"));
        } else {
            titleLabel->setText(guidanceTitle(guidance));
            if (guidance == Guidance::StillMissing)
                hintLabel->setText(qtTrId("lc-agent-guidance-still-missing-hint"));
            else
                hintLabel->clear();
        }
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
    installHelpLabel->setText(
        QStringLiteral("<a href=\"%1\">%2</a>")
            .arg(installHelpUrl.toHtmlEscaped(), qtTrId("lc-agent-state-install-help").toHtmlEscaped()));

    const bool guided = presenceState != PresenceState::Ready;
    titleLabel->setVisible(guided);
    hintLabel->setVisible(guided && !hintLabel->text().isEmpty());
    // Retry is offered in every guided state but the one where a start is
    // already under way: an unavailable agent may answer or be started, and a
    // missing one may have been installed since the window last looked — the
    // client learns that only when asked again, so without the button the
    // window would keep saying "not installed" until it was restarted.
    retryButton->setVisible(guided && guidance != Guidance::Starting);
    installHelpLabel->setVisible(presenceState == PresenceState::AgentMissing && !installHelpUrl.isEmpty());
    setVisible(guided);
}

} // namespace librecelik::agent
