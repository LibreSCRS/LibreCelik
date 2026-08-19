// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class QComboBox;
class QStackedWidget;
class QWidget;

/// @brief The reader selector and the page stack, kept in lockstep.
///
/// The window used to drive a QComboBox and a QStackedWidget directly, from
/// four places, with the card-to-page mapping held in a separate container and
/// the correspondence between the three maintained by hand. Two defects lived
/// in the gaps between them, and neither was reachable by any test, because the
/// window is deliberately excluded from every test binary:
///
///   * removing a card emitted a selector change whose handler read the STACK's
///     index — not yet updated — and so cancelled the in-flight read of a
///     DIFFERENT reader's card;
///   * replacing a page (which happens on the first streamed group of every
///     read) switched the stack unconditionally while the selector stayed
///     where it was, so the window showed one reader and the selector named
///     another.
///
/// This type owns that correspondence instead. It needs no window, no agent and
/// no plugins — two widgets and a map — so it is exercised directly.
///
/// @par Invariant
/// Selector row @c i, stack index @c i and @c cardIds()[i] name the same card,
/// at every observable point. Every mutator restores it before returning.
class ReaderPages : public QObject
{
    Q_OBJECT

public:
    /// @param selector  Reader selector; rows are added and removed here.
    /// @param stack     Page stack; parallel to @p selector.
    ReaderPages(QComboBox* selector, QStackedWidget* stack, QObject* parent = nullptr);

    /// @brief Add a card's page. Takes ownership through the stack.
    /// @note Selects the new page ONLY when it is the first: selecting on every
    ///       insert made a second reader's card abort the first reader's read,
    ///       because leaving a page cancels its read.
    void add(const QString& cardId, const QString& readerLabel, QWidget* page);

    /// @brief Drop a card's page. Emits no @ref leftCard — teardown is not
    ///        navigation, and the caller cancels the departing card itself.
    void remove(const QString& cardId);

    /// @brief Swap a card's page in place, keeping its position.
    /// @note Changes what is VISIBLE only when the replaced page is the one on
    ///       screen. A background reader streaming its first group must not
    ///       pull the view off the reader the user is looking at.
    void replace(const QString& cardId, QWidget* newPage);

    [[nodiscard]] bool contains(const QString& cardId) const;
    [[nodiscard]] QWidget* page(const QString& cardId) const;
    [[nodiscard]] int count() const;
    [[nodiscard]] bool isEmpty() const;

    /// @brief Card whose page is on screen, or empty when there is none.
    [[nodiscard]] QString currentCardId() const;

    /// @brief Cards in selector/stack order. Position @c i is selector row @c i.
    [[nodiscard]] QStringList cardIds() const;

Q_SIGNALS:
    /// @brief The user navigated AWAY from @p cardId, whose read must stop.
    /// Emitted only for a real selection change, never for add or remove.
    void leftCard(const QString& cardId);

private:
    void onSelectorChanged(int index);

    QComboBox* m_selector = nullptr;
    QStackedWidget* m_stack = nullptr;
    QStringList m_order;              ///< selector/stack position -> cardId
    QHash<QString, QWidget*> m_pages; ///< cardId -> page
};
