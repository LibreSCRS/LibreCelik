// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "pkswidgetplugin.h"
#include "pkswidget.h"

QWidget* PksWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    return new PksWidget(data, parent);
}
