// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "rshealthwidgetplugin.h"
#include "healthwidget.h"
#include "healthtextdocument.h"
#include "utils/printmanager.h"

QWidget* RsHealthWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    auto* w = new HealthWidget(data, parent);
    connect(w, &HealthWidget::printRequested, this, [this](const plugin::CardData& d) { print(d); });
    return w;
}

QWidget* RsHealthWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new HealthWidget(parent);
    connect(w, &HealthWidget::printRequested, this, [this](const plugin::CardData& d) { print(d); });
    return w;
}

void RsHealthWidgetPlugin::print(const plugin::CardData& data) const
{
    HealthTextDocument doc(data);
    PrintManager::printDocument(doc, qtTrId("lc-health-print-title"));
}

void RsHealthWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* hw = qobject_cast<HealthWidget*>(widget)) {
        hw->addGroup(group);
    }
}
