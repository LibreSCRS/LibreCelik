// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
#include "readerpages.h"

#include <QComboBox>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QWidget>

ReaderPages::ReaderPages(QComboBox* selector, QStackedWidget* stack, QObject* parent)
    : QObject(parent), m_selector(selector), m_stack(stack)
{
    connect(m_selector, &QComboBox::currentIndexChanged, this, &ReaderPages::onSelectorChanged);
}

void ReaderPages::add(const QString& cardId, const QString& readerLabel, QWidget* page)
{
    if (m_pages.contains(cardId)) {
        return;
    }
    // The selector must not announce a change while the two halves are
    // mid-update: the handler would read a stack that has not caught up.
    {
        const QSignalBlocker block(m_selector);
        m_stack->addWidget(page);
        m_selector->addItem(readerLabel);
    }
    m_order.append(cardId);
    m_pages.insert(cardId, page);

    // First card only — see the header. Set both halves explicitly rather than
    // letting the blocked selector drive the stack.
    if (m_order.size() == 1) {
        const QSignalBlocker block(m_selector);
        m_selector->setCurrentIndex(0);
        m_stack->setCurrentIndex(0);
    }
}

void ReaderPages::remove(const QString& cardId)
{
    const int pos = m_order.indexOf(cardId);
    if (pos < 0) {
        return;
    }
    QWidget* widget = m_pages.value(cardId);
    const QString wasCurrent = currentCardId();

    // Teardown, not navigation. Removing a selector row below the current one
    // renumbers it and makes QComboBox emit currentIndexChanged; the handler
    // would then read a stack index that still refers to the pre-removal
    // layout and cancel a bystander's read. Block the selector across the whole
    // teardown and reconcile the selection deliberately afterwards.
    {
        const QSignalBlocker block(m_selector);
        m_selector->removeItem(pos);
        if (widget != nullptr) {
            m_stack->removeWidget(widget);
            widget->deleteLater();
        }
        m_order.removeAt(pos);
        m_pages.remove(cardId);

        // Keep showing whatever the user was looking at; fall back to the
        // neighbour that slid into the vacated position when it was the one
        // that left.
        int selected = -1;
        if (wasCurrent != cardId) {
            selected = m_order.indexOf(wasCurrent);
        }
        if (selected < 0 && !m_order.isEmpty()) {
            selected = qMin(pos, static_cast<int>(m_order.size()) - 1);
        }
        m_selector->setCurrentIndex(selected);
        m_stack->setCurrentIndex(selected);
    }
}

void ReaderPages::replace(const QString& cardId, QWidget* newPage)
{
    const int pos = m_order.indexOf(cardId);
    if (pos < 0) {
        newPage->deleteLater();
        return;
    }
    QWidget* oldWidget = m_pages.value(cardId);
    const bool wasVisible = (m_stack->currentIndex() == pos);

    {
        const QSignalBlocker block(m_selector);
        if (oldWidget != nullptr) {
            m_stack->removeWidget(oldWidget);
            oldWidget->deleteLater();
        }
        m_stack->insertWidget(pos, newPage);
        // Only follow the swap when it happened on the visible page. A reader
        // streaming its first group in the background must not steal the view.
        if (wasVisible) {
            m_stack->setCurrentIndex(pos);
            m_selector->setCurrentIndex(pos);
        } else {
            // insertWidget can shift what the stack considers current; restore
            // the stack to whatever the selector still names.
            m_stack->setCurrentIndex(m_selector->currentIndex());
        }
    }
    m_pages.insert(cardId, newPage);
}

bool ReaderPages::contains(const QString& cardId) const
{
    return m_pages.contains(cardId);
}

QWidget* ReaderPages::page(const QString& cardId) const
{
    return m_pages.value(cardId, nullptr);
}

int ReaderPages::count() const
{
    return static_cast<int>(m_order.size());
}

bool ReaderPages::isEmpty() const
{
    return m_order.isEmpty();
}

QString ReaderPages::currentCardId() const
{
    const int index = m_stack->currentIndex();
    if (index < 0 || index >= m_order.size()) {
        return {};
    }
    return m_order.at(index);
}

QStringList ReaderPages::cardIds() const
{
    return m_order;
}

void ReaderPages::onSelectorChanged(int index)
{
    // Reached only for a genuine selection change: every mutator above blocks
    // this while it is repairing the correspondence.
    const int leaving = m_stack->currentIndex();
    if (leaving >= 0 && leaving != index && leaving < m_order.size()) {
        Q_EMIT leftCard(m_order.at(leaving));
    }
    m_stack->setCurrentIndex(index);
}
