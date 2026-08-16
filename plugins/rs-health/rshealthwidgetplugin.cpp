// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "rshealthwidgetplugin.h"
#include "healthwidget.h"
#include "healthtextdocument.h"
#include "utils/printmanager.h"

using LibreSCRS::AgentClient::FieldGroup;

QWidget* RsHealthWidgetPlugin::createWidget(const QList<FieldGroup>& groups, QWidget* parent) const
{
    auto* w = new HealthWidget(groups, parent);
    connect(w, &HealthWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

QWidget* RsHealthWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new HealthWidget(parent);
    connect(w, &HealthWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

void RsHealthWidgetPlugin::print(const QList<FieldGroup>& groups) const
{
    HealthTextDocument doc(groups);
    // clang-format off
    const auto t = qtTrId("lc-health-print-title"); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
    // clang-format on
    PrintManager::printDocument(doc, t);
}

void RsHealthWidgetPlugin::addGroup(const FieldGroup& group, QWidget* widget) const
{
    if (auto* hw = qobject_cast<HealthWidget*>(widget)) {
        hw->addGroup(group);
    }
}
