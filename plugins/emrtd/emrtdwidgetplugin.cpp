// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdwidgetplugin.h"
#include "emrtdwidget.h"

QWidget* EMRTDWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    return new EMRTDWidget(data, parent);
}
