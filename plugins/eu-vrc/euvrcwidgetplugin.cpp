// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "euvrcwidgetplugin.h"
#include "euvrcwidget.h"
#include "euvrctextdocument.h"
#include "utils/printmanager.h"

using LibreSCRS::AgentClient::FieldGroup;

QWidget* EuVrcWidgetPlugin::createWidget(const QList<FieldGroup>& groups, QWidget* parent) const
{
    auto* w = new EuVrcWidget(groups, parent);
    connect(w, &EuVrcWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

QWidget* EuVrcWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new EuVrcWidget(parent);
    connect(w, &EuVrcWidget::printRequested, this, [this](const QList<FieldGroup>& g) { print(g); });
    return w;
}

void EuVrcWidgetPlugin::print(const QList<FieldGroup>& groups) const
{
    EuVrcTextDocument doc(groups);
    // clang-format off
    const auto t = qtTrId("lc-euvrc-print-title"); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
    // clang-format on
    PrintManager::printDocument(doc, t);
}

void EuVrcWidgetPlugin::addGroup(const FieldGroup& group, QWidget* widget) const
{
    if (auto* euVrcWidget = qobject_cast<EuVrcWidget*>(widget))
        euVrcWidget->addGroup(group);
}
