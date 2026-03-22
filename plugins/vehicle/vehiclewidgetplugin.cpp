// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "vehiclewidgetplugin.h"
#include "vehiclewidget.h"

QWidget* VehicleWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    return new VehicleWidget(data, parent);
}

QWidget* VehicleWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    return new VehicleWidget(parent);
}

void VehicleWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* vehicleWidget = qobject_cast<VehicleWidget*>(widget))
        vehicleWidget->addGroup(group);
}
