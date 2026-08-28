// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/cardretrypage.h"

#include "utils/messageline.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace librecelik::agent {

using librecelik::utils::addMessageLine;

CardRetryPage::CardRetryPage(QString failure, QString reader, QWidget* parent)
    : QWidget(parent), failureText(std::move(failure)), readerName(std::move(reader))
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // Test seams, and how a manual smoke tells a page that rendered nothing
    // from a page that was never asked to render. addMessageLine also pins
    // Qt::PlainText — two of these lines substitute text that came off a card
    // or out of the agent, and under the default AutoText a value with angle
    // brackets in it would be parsed as HTML and partly disappear from the
    // line that exists to explain the failure.

    // Centred by STRETCHES rather than by an alignment on the layout, for the
    // reason the unreadable-card page documents: an aligned layout takes its
    // own size hint instead of the page's, so a wrapped label is sized at a
    // width it does not end up wrapping at and loses its last lines.
    layout->addStretch();
    titleLabel = addMessageLine(layout, this, QStringLiteral("cardRetryTitle"), /*bold=*/true);
    readerLabel = addMessageLine(layout, this, QStringLiteral("cardRetryReader"));
    reasonLabel = addMessageLine(layout, this, QStringLiteral("cardRetryReason"));

    retryButton = new QPushButton(this);
    retryButton->setObjectName(QStringLiteral("cardRetryButton"));
    connect(retryButton, &QPushButton::clicked, this, &CardRetryPage::retryRequested);
    layout->addWidget(retryButton, 0, Qt::AlignCenter);
    layout->addStretch();

    retranslateUi();
}

void CardRetryPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void CardRetryPage::retranslateUi()
{
    titleLabel->setText(qtTrId("lc-card-retry-title"));
    readerLabel->setText(qtTrId("lc-card-unsupported-reader").arg(readerName));
    // The agent's line is the only specific record of WHICH failure happened;
    // an empty one leaves the headline to speak alone rather than showing a
    // blank row where a reason belongs.
    reasonLabel->setText(failureText);
    reasonLabel->setVisible(!failureText.isEmpty());
    retryButton->setText(qtTrId("lc-card-retry-button"));
}

} // namespace librecelik::agent
