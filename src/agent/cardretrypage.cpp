// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/cardretrypage.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace librecelik::agent {

CardRetryPage::CardRetryPage(QString failure, QString reader, QWidget* parent)
    : QWidget(parent), failureText(std::move(failure)), readerName(std::move(reader))
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // Test seams, and how a manual smoke tells a page that rendered nothing
    // from a page that was never asked to render.
    auto addLine = [this, layout](const QString& objectName, bool bold) {
        auto* label = new QLabel(this);
        label->setObjectName(objectName);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        // None of these lines carries markup, and two of them substitute text
        // that came off a card or out of the agent — under the default AutoText
        // a value with angle brackets in it would be parsed as HTML and partly
        // disappear from the line that exists to explain the failure.
        label->setTextFormat(Qt::PlainText);
        if (bold) {
            label->setStyleSheet(QStringLiteral("QLabel { font-size: 14px; font-weight: bold; }"));
        }
        layout->addWidget(label);
        return label;
    };

    // Centred by STRETCHES rather than by an alignment on the layout, for the
    // reason the unreadable-card page documents: an aligned layout takes its
    // own size hint instead of the page's, so a wrapped label is sized at a
    // width it does not end up wrapping at and loses its last lines.
    layout->addStretch();
    titleLabel = addLine(QStringLiteral("cardRetryTitle"), /*bold=*/true);
    readerLabel = addLine(QStringLiteral("cardRetryReader"), /*bold=*/false);
    reasonLabel = addLine(QStringLiteral("cardRetryReason"), /*bold=*/false);

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
