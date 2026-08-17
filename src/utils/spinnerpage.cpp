// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/spinnerpage.h"

#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QWidget>

namespace librecelik::utils {

namespace {

/// The property the page is recognised by. Written and read in this file only,
/// so the two can never drift apart.
constexpr char kSpinnerProperty[] = "isSpinner";

/// The indeterminate bar's width. Fixed so the bar reads as a spinner rather
/// than as progress across the page — and given to the BAR alone, never to the
/// column, which the guided text below it needs in full.
constexpr int kBarWidth = 200;

} // namespace

QWidget* makeSpinnerPage(const QString& text, QWidget* parent)
{
    auto* spinnerWidget = new QWidget(parent);
    spinnerWidget->setProperty(kSpinnerProperty, true);
    auto* layout = new QVBoxLayout(spinnerWidget);
    auto* bar = new QProgressBar(spinnerWidget);
    bar->setRange(0, 0);
    bar->setFixedWidth(kBarWidth);
    bar->setTextVisible(false);
    auto* label = new QLabel(text, spinnerWidget);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);

    // Centred by STRETCHES, not by an alignment on the layout itself, and the
    // bar is centred as an item rather than by sizing the whole column to it.
    //
    // Both matter to a wrapped label. An aligned layout takes only its own
    // size hint instead of the page, so the label is handed a height computed
    // at some other width than the one it ends up wrapping at — and a fixed
    // bar width caps the column, so that width was 200 px however wide the
    // page grew. Either way the guided text lost its last lines. With the
    // layout holding the whole page, the box layout's height-for-width pass
    // asks the label how tall it must be AT ITS REAL WIDTH and gives it that;
    // the stretches take what is left, so a one-line message still sits in the
    // middle rather than filling the page.
    layout->addStretch();
    layout->addWidget(bar, 0, Qt::AlignHCenter);
    layout->addWidget(label);
    layout->addStretch();
    return spinnerWidget;
}

bool isSpinner(QWidget* widget)
{
    return widget != nullptr && widget->property(kSpinnerProperty).toBool();
}

} // namespace librecelik::utils
