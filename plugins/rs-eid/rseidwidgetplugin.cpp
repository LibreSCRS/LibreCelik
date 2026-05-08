// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "rseidwidgetplugin.h"
#include "eidwidget.h"
#include "eidtextdocument.h"
#include "utils/printmanager.h"

QWidget* RsEidWidgetPlugin::createWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent) const
{
    auto* w = new EidWidget(data, parent);
    connect(w, &EidWidget::printRequested, this, [this](const LibreSCRS::Plugin::CardData& d) { print(d); });
    return w;
}

QWidget* RsEidWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new EidWidget(parent);
    connect(w, &EidWidget::printRequested, this, [this](const LibreSCRS::Plugin::CardData& d) { print(d); });
    return w;
}

void RsEidWidgetPlugin::print(const LibreSCRS::Plugin::CardData& data) const
{
    EIdTextDocument doc(data);
    PrintManager::printDocument(
        doc,
        qtTrId("lc-eid-print-title")); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
}

void RsEidWidgetPlugin::addGroup(const LibreSCRS::Plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* w = qobject_cast<EidWidget*>(widget))
        w->addGroup(group);
}
