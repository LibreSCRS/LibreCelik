// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "tokenwidgetplugin.h"

#include <QVBoxLayout>

QWidget* TokenWidgetPlugin::createWidget(const plugin::CardData& /*data*/, QWidget* parent) const
{
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    return widget;
}
