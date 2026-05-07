// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "euvrcwidgetplugin.h"
#include "euvrcwidget.h"
#include "euvrctextdocument.h"
#include "utils/printmanager.h"

QWidget* EuVrcWidgetPlugin::createWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent) const
{
    auto* w = new EuVrcWidget(data, parent);
    connect(w, &EuVrcWidget::printRequested, this, [this](const LibreSCRS::Plugin::CardData& d) { print(d); });
    return w;
}

QWidget* EuVrcWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new EuVrcWidget(parent);
    connect(w, &EuVrcWidget::printRequested, this, [this](const LibreSCRS::Plugin::CardData& d) { print(d); });
    return w;
}

void EuVrcWidgetPlugin::print(const LibreSCRS::Plugin::CardData& data) const
{
    EuVrcTextDocument doc(data);
    PrintManager::printDocument(doc, qtTrId("lc-euvrc-print-title"));
}

void EuVrcWidgetPlugin::addGroup(const LibreSCRS::Plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* euVrcWidget = qobject_cast<EuVrcWidget*>(widget))
        euVrcWidget->addGroup(group);
}
