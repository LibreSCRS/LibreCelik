// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "pkswidget.h"
#include <QVBoxLayout>

PksWidget::PksWidget(const plugin::CardData& /*data*/, QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
}
