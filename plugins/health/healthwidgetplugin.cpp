// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "healthwidgetplugin.h"
#include "healthwidget.h"

QWidget* HealthWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    return new HealthWidget(data, parent);
}

QWidget* HealthWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    return new HealthWidget(parent);
}

void HealthWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* hw = qobject_cast<HealthWidget*>(widget)) {
        hw->addGroup(group);
    }
}
