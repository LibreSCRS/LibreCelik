// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "vehiclewidgetplugin.h"
#include "vehiclewidget.h"
#include "vehicletextdocument.h"
#include "utils/printmanager.h"

QWidget* VehicleWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    auto* w = new VehicleWidget(data, parent);
    connect(w, &VehicleWidget::printRequested, this, [this](const plugin::CardData& d) { print(d, nullptr); });
    return w;
}

QWidget* VehicleWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new VehicleWidget(parent);
    connect(w, &VehicleWidget::printRequested, this, [this](const plugin::CardData& d) { print(d, nullptr); });
    return w;
}

void VehicleWidgetPlugin::print(const plugin::CardData& data, QPrinter* /*printer*/) const
{
    VehicleTextDocument doc(data);
    PrintManager::printDocument(doc, qtTrId("lc-vehicle-print-title"));
}

void VehicleWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* vehicleWidget = qobject_cast<VehicleWidget*>(widget))
        vehicleWidget->addGroup(group);
}
