// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "emrtdwidgetplugin.h"
#include "emrtdwidget.h"
#include "emrtdtextdocument.h"
#include "utils/printmanager.h"

QWidget* EMRTDWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    auto* w = new EMRTDWidget(data, parent);
    connect(w, &EMRTDWidget::printRequested, this, [this](const plugin::CardData& d) { print(d); });
    return w;
}

QWidget* EMRTDWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new EMRTDWidget(parent);
    connect(w, &EMRTDWidget::printRequested, this, [this](const plugin::CardData& d) { print(d); });
    return w;
}

void EMRTDWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* w = qobject_cast<EMRTDWidget*>(widget))
        w->addGroup(group);
}

void EMRTDWidgetPlugin::showNoDataMessage(QWidget* widget) const
{
    if (auto* w = qobject_cast<EMRTDWidget*>(widget))
        w->showNoDataMessage();
}

void EMRTDWidgetPlugin::print(const plugin::CardData& data) const
{
    EMRTDTextDocument doc(data);
    PrintManager::printDocument(doc, qtTrId("lc-emrtd-doc-title"));
}
