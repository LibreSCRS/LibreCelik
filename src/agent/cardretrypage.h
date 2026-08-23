// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The page a card gets when its read failed in a way the holder can
///        clear.
///
/// The window drops a card's page when a read fails while the page is still
/// the spinner, and refuses to re-add the card until it is physically
/// re-seated. That is right for a failure repeating cannot fix. It is wrong
/// for one the HOLDER can clear — an entry window that expired, a dismissed
/// prompt, a mistyped secret — because it removes the very surface they would
/// clear it on.
///
/// Not latching those was the first half of the fix, and on its own it left
/// something worse than either: the page kept spinning under "confirm the
/// access number in the system window" when no such window existed any more,
/// while the status bar said to try again and nothing on screen could.
///
/// This page is the other half. It stops the spinner, says what happened in
/// the agent's own words, names the reader it belongs to (the reader selector
/// is hidden while there is only one page, so the page is the only place that
/// fact can appear), and offers the one thing that helps: read again.

#pragma once

#include <QString>
#include <QWidget>

class QEvent;
class QLabel;
class QPushButton;

namespace librecelik::agent {

/// The per-reader page for a read that failed recoverably.
///
/// Holds the failure text and reader name as DATA rather than the sentences
/// built from them, so a language change re-renders through the catalog
/// instead of freezing the page in the language it was built in.
class CardRetryPage : public QWidget
{
    Q_OBJECT
public:
    /// @param failure The localized line the controller surfaced. May be empty,
    ///                in which case only the generic headline is shown.
    /// @param reader  The reader's display name, as the window resolves it.
    CardRetryPage(QString failure, QString reader, QWidget* parent = nullptr);

signals:
    /// The holder asked for another read of this card.
    void retryRequested();

protected:
    void changeEvent(QEvent* event) override; // retranslate

private:
    void retranslateUi();

    QString failureText;
    QString readerName;

    QLabel* titleLabel = nullptr;
    QLabel* readerLabel = nullptr;
    QLabel* reasonLabel = nullptr;
    QPushButton* retryButton = nullptr;
};

} // namespace librecelik::agent
