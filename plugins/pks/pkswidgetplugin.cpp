// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "pkswidgetplugin.h"
#include "pkswidget.h"

QWidget* PksWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    return new PksWidget(data, parent);
}

QWidget* PksWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    return new PksWidget(parent);
}

void PksWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* pks = qobject_cast<PksWidget*>(widget)) {
        pks->addGroup(group);
    }
}
