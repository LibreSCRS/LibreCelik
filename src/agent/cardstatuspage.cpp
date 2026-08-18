// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/cardstatuspage.h"

#include <QEvent>
#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

#include <utility>

namespace librecelik::agent {

QString atrSnippet(const QString& atrHex, qsizetype maxBytes)
{
    QStringList bytes;
    for (qsizetype offset = 0; offset < atrHex.size() && bytes.size() < maxBytes; offset += 2)
        bytes << atrHex.mid(offset, 2).toUpper();
    QString out = bytes.join(QLatin1Char(' '));
    if (bytes.size() * 2 < atrHex.size())
        out += QStringLiteral(" ...");
    return out;
}

CardStatusPage::CardStatusPage(LibreSCRS::AgentClient::UiState state, QString reader, QString atr, QWidget* parent)
    : QWidget(parent), cardState(state), readerName(std::move(reader)), atrHex(std::move(atr))
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // Test seams: the offscreen suite reads each rendered line back through
    // these names, and they are also how a manual smoke tells a page that
    // rendered nothing apart from a page that was never asked to render.
    auto addLine = [this, layout](const QString& objectName, bool bold) {
        auto* label = new QLabel(this);
        label->setObjectName(objectName);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        // None of these lines carries markup, and one of them substitutes a
        // reader name the agent supplies — under the default AutoText a name
        // with angle brackets in it would be parsed as HTML and partly
        // disappear from the very line that exists to identify the reader.
        label->setTextFormat(Qt::PlainText);
        if (bold)
            label->setStyleSheet(QStringLiteral("QLabel { font-size: 14px; font-weight: bold; }"));
        layout->addWidget(label);
        return label;
    };

    // Centred by STRETCHES rather than by an alignment on the layout itself,
    // for the reason the read-in-progress page documents: an aligned layout
    // takes its own size hint instead of the page, so a wrapped label is sized
    // at a width it does not end up wrapping at and loses its last lines. The
    // interface hint is several sentences long and is exactly the text a
    // clipped page would swallow.
    layout->addStretch();
    titleLabel = addLine(QStringLiteral("cardStatusTitle"), /*bold=*/true);
    readerLabel = addLine(QStringLiteral("cardStatusReader"), /*bold=*/false);
    hintLabel = addLine(QStringLiteral("cardStatusHint"), /*bold=*/false);
    layout->addStretch();

    retranslateUi();
}

void CardStatusPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CardStatusPage::retranslateUi()
{
    // UnknownCard is the only state with a verdict to quote: no driver matched
    // the ATR, so the ATR is what the holder — and a bug report — has to go
    // on. Error means a driver DID match, so that card's ATR proves nothing
    // about why it renders nothing.
    titleLabel->setText(cardState == LibreSCRS::AgentClient::UiState::UnknownCard
                            ? qtTrId("lc-reader-unsupported-card-with-atr").arg(atrSnippet(atrHex))
                            : qtTrId("lc-reader-unsupported-card"));
    readerLabel->setText(qtTrId("lc-card-unsupported-reader").arg(readerName));
    hintLabel->setText(qtTrId("lc-card-unsupported-interface-hint"));
}

} // namespace librecelik::agent
