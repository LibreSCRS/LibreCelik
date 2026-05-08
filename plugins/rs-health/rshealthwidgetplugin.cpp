// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "rshealthwidgetplugin.h"
#include "healthwidget.h"
#include "healthtextdocument.h"
#include "utils/printmanager.h"

QWidget* RsHealthWidgetPlugin::createWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent) const
{
    auto* w = new HealthWidget(data, parent);
    connect(w, &HealthWidget::printRequested, this, [this](const LibreSCRS::Plugin::CardData& d) { print(d); });
    return w;
}

QWidget* RsHealthWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new HealthWidget(parent);
    connect(w, &HealthWidget::printRequested, this, [this](const LibreSCRS::Plugin::CardData& d) { print(d); });
    return w;
}

void RsHealthWidgetPlugin::print(const LibreSCRS::Plugin::CardData& data) const
{
    HealthTextDocument doc(data);
    // clang-format off
    const auto t = qtTrId("lc-health-print-title"); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
    // clang-format on
    PrintManager::printDocument(doc, t);
}

void RsHealthWidgetPlugin::addGroup(const LibreSCRS::Plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* hw = qobject_cast<HealthWidget*>(widget)) {
        hw->addGroup(group);
    }
}
