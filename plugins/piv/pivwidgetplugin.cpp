// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "pivwidgetplugin.h"
#include "pivwidget.h"
#include "pivtextdocument.h"
#include "utils/printmanager.h"

using LibreSCRS::AgentClient::FieldGroup;

QWidget* PIVWidgetPlugin::createWidget(const QList<FieldGroup>& groups, QWidget* parent) const
{
    auto* w = new PIVWidget(groups, parent);
    connect(w, &PIVWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

QWidget* PIVWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new PIVWidget(parent);
    connect(w, &PIVWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

void PIVWidgetPlugin::print(const QList<FieldGroup>& groups) const
{
    PIVTextDocument doc(groups);
    PrintManager::printDocument(
        doc,
        qtTrId("lc-piv-print-title")); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
}

void PIVWidgetPlugin::addGroup(const FieldGroup& group, QWidget* widget) const
{
    if (auto* pivWidget = qobject_cast<PIVWidget*>(widget))
        pivWidget->addGroup(group);
}
