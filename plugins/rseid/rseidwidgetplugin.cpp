// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "rseidwidgetplugin.h"
#include "eidwidget.h"
#include "eidtextdocument.h"
#include "utils/printmanager.h"

QWidget* RsEidWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    auto* w = new EidWidget(data, parent);
    connect(w, &EidWidget::printRequested, this, [this](const plugin::CardData& d) { print(d, nullptr); });
    return w;
}

QWidget* RsEidWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new EidWidget(parent);
    connect(w, &EidWidget::printRequested, this, [this](const plugin::CardData& d) { print(d, nullptr); });
    return w;
}

void RsEidWidgetPlugin::print(const plugin::CardData& data, QPrinter* /*printer*/) const
{
    EIdTextDocument doc(data);
    PrintManager::printDocument(doc, qtTrId("lc-eid-print-title"));
}

void RsEidWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* w = qobject_cast<EidWidget*>(widget))
        w->addGroup(group);
}
