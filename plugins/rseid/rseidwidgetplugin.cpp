// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "rseidwidgetplugin.h"
#include "eidwidget.h"

QWidget* RsEidWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    return new EidWidget(data, parent);
}

QWidget* RsEidWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    return new EidWidget(parent);
}

void RsEidWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* w = qobject_cast<EidWidget*>(widget))
        w->addGroup(group);
}
