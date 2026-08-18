// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The page a card gets when the read never produces one.
///
/// A card the agent matched no driver for, and a card whose driver creates no
/// user-visible surface at all, both end the add path with nothing to render.
/// They used to end it with a line on the window's single status bar, which is
/// the wrong surface twice over: the bar is global, so the next readable card
/// wiped the notice, and two readers with two unreadable cards could only ever
/// show one of them. Worse, the card got no entry in the reader selector at
/// all, so a holder with a card in each of two readers saw one page and no
/// trace of the other reader.
///
/// So an unreadable card gets a PAGE, in the same stack and the same selector
/// every readable card uses. The page names its own reader — the selector is
/// hidden while there is only one page, so the page is the only place that
/// fact can appear — reports what the card was judged on, and offers the one
/// thing that genuinely resolves the common case: a dual-interface document
/// whose contact and contactless sides expose different applets, tapped on the
/// side that carries nothing this build can read.
///
/// The decision itself is a pure predicate over the resolved `UiState`, kept
/// out of the window's glue so the CI test drives the same code production
/// does (the `optionalsections.h` / `plugintyperesolution.h` pattern).

#pragma once

#include <LibreSCRS/AgentClient/AgentCapabilities.h> // UiState

#include <QString>
#include <QWidget>

class QEvent;
class QLabel;

namespace librecelik::agent {

/// Whether @p state leaves the card with no page of its own to build.
///
/// `UnknownCard` (the agent matched no driver, having already had full APDU
/// access — a definitive verdict, not a transient one) and `Error` (a driver
/// matched, but its capabilities are ancillary-only) are the two. Every other
/// state has something to read and takes the ordinary spinner-then-plugin
/// path; nothing here retries, because in both cases there is nothing left to
/// try against this card on this interface.
[[nodiscard]] constexpr bool hasNoCardSurface(LibreSCRS::AgentClient::UiState state) noexcept
{
    return state == LibreSCRS::AgentClient::UiState::UnknownCard || state == LibreSCRS::AgentClient::UiState::Error;
}

/// Display form of the ATR the agent reports for an unrecognised card: the
/// leading @p maxBytes bytes, space-separated and upper-case, with a marker
/// when the ATR is longer.
///
/// The agent hands the ATR over as a flat hex string, so this is a regrouping
/// of characters rather than a formatting of bytes. An odd-length input (which
/// the client contract does not produce) keeps its trailing nibble rather than
/// silently dropping it.
[[nodiscard]] QString atrSnippet(const QString& atrHex, qsizetype maxBytes = 6);

/// The per-reader page for a card that produced no readable surface.
///
/// It holds the card's state, reader name and ATR rather than the sentences
/// built from them, so a language change re-renders through the catalog
/// instead of freezing the page in the language it was built in.
class CardStatusPage : public QWidget
{
    Q_OBJECT
public:
    /// @param state  The card's resolved state; must satisfy hasNoCardSurface.
    /// @param reader The reader's display name, as the window resolves it.
    /// @param atr    The card's ATR as flat hex, reported for `UnknownCard`.
    CardStatusPage(LibreSCRS::AgentClient::UiState state, QString reader, QString atr, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override; // retranslate

private:
    void retranslateUi();

    LibreSCRS::AgentClient::UiState cardState;
    QString readerName;
    QString atrHex;

    QLabel* titleLabel = nullptr;
    QLabel* readerLabel = nullptr;
    QLabel* hintLabel = nullptr;
};

} // namespace librecelik::agent
