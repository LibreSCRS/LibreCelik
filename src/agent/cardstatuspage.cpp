// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/cardstatuspage.h"

#include "utils/messageline.h"

#include <QEvent>
#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

#include <utility>

namespace librecelik::agent {

using librecelik::utils::addMessageLine;

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
    // addMessageLine also pins Qt::PlainText — one of these lines substitutes
    // a reader name the agent supplies, and under the default AutoText a name
    // with angle brackets in it would be parsed as HTML and partly disappear
    // from the very line that exists to identify the reader.

    // Centred by STRETCHES rather than by an alignment on the layout itself,
    // for the reason the read-in-progress page documents: an aligned layout
    // takes its own size hint instead of the page, so a wrapped label is sized
    // at a width it does not end up wrapping at and loses its last lines. The
    // interface hint is several sentences long and is exactly the text a
    // clipped page would swallow.
    layout->addStretch();
    titleLabel = addMessageLine(layout, this, QStringLiteral("cardStatusTitle"), /*bold=*/true);
    readerLabel = addMessageLine(layout, this, QStringLiteral("cardStatusReader"));
    hintLabel = addMessageLine(layout, this, QStringLiteral("cardStatusHint"));
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
    // about why it renders nothing. An UnknownCard whose session never
    // surfaced an ATR has nothing to quote either — same no-ATR sentence,
    // not a sentence with a hole in it.
    const QString snippet = atrSnippet(atrHex);
    titleLabel->setText(cardState == LibreSCRS::AgentClient::UiState::UnknownCard && !snippet.isEmpty()
                            ? qtTrId("lc-reader-unsupported-card-with-atr").arg(snippet)
                            : qtTrId("lc-reader-unsupported-card"));
    readerLabel->setText(qtTrId("lc-card-unsupported-reader").arg(readerName));
    hintLabel->setText(qtTrId("lc-card-unsupported-interface-hint"));
}

} // namespace librecelik::agent
