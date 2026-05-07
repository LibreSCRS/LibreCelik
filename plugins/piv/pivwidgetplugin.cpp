// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "pivwidgetplugin.h"
#include "pivwidget.h"
#include "pivtextdocument.h"
#include "utils/printmanager.h"

QWidget* PIVWidgetPlugin::createWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent) const
{
    auto* w = new PIVWidget(data, parent);
    connect(w, &PIVWidget::printRequested, this, [this](const LibreSCRS::Plugin::CardData& d) { print(d); });
    return w;
}

QWidget* PIVWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new PIVWidget(parent);
    connect(w, &PIVWidget::printRequested, this, [this](const LibreSCRS::Plugin::CardData& d) { print(d); });
    return w;
}

void PIVWidgetPlugin::print(const LibreSCRS::Plugin::CardData& data) const
{
    PIVTextDocument doc(data);
    PrintManager::printDocument(doc, qtTrId("lc-piv-print-title"));
}

void PIVWidgetPlugin::addGroup(const LibreSCRS::Plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* pivWidget = qobject_cast<PIVWidget*>(widget))
        pivWidget->addGroup(group);
}
