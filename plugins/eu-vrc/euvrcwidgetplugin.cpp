// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "euvrcwidgetplugin.h"
#include "euvrcwidget.h"
#include "euvrctextdocument.h"
#include "utils/printmanager.h"

QWidget* EuVrcWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    auto* w = new EuVrcWidget(data, parent);
    connect(w, &EuVrcWidget::printRequested, this, [this](const plugin::CardData& d) { print(d); });
    return w;
}

QWidget* EuVrcWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new EuVrcWidget(parent);
    connect(w, &EuVrcWidget::printRequested, this, [this](const plugin::CardData& d) { print(d); });
    return w;
}

void EuVrcWidgetPlugin::print(const plugin::CardData& data) const
{
    EuVrcTextDocument doc(data);
    PrintManager::printDocument(doc, qtTrId("lc-euvrc-print-title"));
}

void EuVrcWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* euVrcWidget = qobject_cast<EuVrcWidget*>(widget))
        euVrcWidget->addGroup(group);
}
