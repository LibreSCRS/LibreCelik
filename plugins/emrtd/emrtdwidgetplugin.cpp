// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "emrtdwidgetplugin.h"
#include "emrtdwidget.h"
#include "emrtdtextdocument.h"
#include "utils/printmanager.h"

using LibreSCRS::AgentClient::FieldGroup;

QWidget* EMRTDWidgetPlugin::createWidget(const QList<FieldGroup>& groups, QWidget* parent) const
{
    auto* w = new EMRTDWidget(groups, parent);
    connect(w, &EMRTDWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

QWidget* EMRTDWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new EMRTDWidget(parent);
    connect(w, &EMRTDWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

void EMRTDWidgetPlugin::addGroup(const FieldGroup& group, QWidget* widget) const
{
    if (auto* w = qobject_cast<EMRTDWidget*>(widget))
        w->addGroup(group);
}

void EMRTDWidgetPlugin::showNoDataMessage(QWidget* widget) const
{
    if (auto* w = qobject_cast<EMRTDWidget*>(widget))
        w->showNoDataMessage();
}

void EMRTDWidgetPlugin::print(const QList<FieldGroup>& groups) const
{
    EMRTDTextDocument doc(groups);
    PrintManager::printDocument(
        doc,
        qtTrId("lc-emrtd-doc-title")); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
}
