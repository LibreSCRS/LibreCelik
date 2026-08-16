// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "rseidwidgetplugin.h"
#include "eidwidget.h"
#include "eidtextdocument.h"
#include "utils/printmanager.h"

using LibreSCRS::AgentClient::FieldGroup;

QWidget* RsEidWidgetPlugin::createWidget(const QList<FieldGroup>& groups, QWidget* parent) const
{
    auto* w = new EidWidget(groups, parent);
    connect(w, &EidWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

QWidget* RsEidWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new EidWidget(parent);
    connect(w, &EidWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

void RsEidWidgetPlugin::print(const QList<FieldGroup>& groups) const
{
    EIdTextDocument doc(groups);
    PrintManager::printDocument(
        doc,
        qtTrId("lc-eid-print-title")); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
}

void RsEidWidgetPlugin::addGroup(const FieldGroup& group, QWidget* widget) const
{
    if (auto* w = qobject_cast<EidWidget*>(widget))
        w->addGroup(group);
}
