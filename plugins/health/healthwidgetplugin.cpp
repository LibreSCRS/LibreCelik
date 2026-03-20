// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "healthwidgetplugin.h"
#include "healthwidget.h"

QWidget* HealthWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    return new HealthWidget(data, parent);
}