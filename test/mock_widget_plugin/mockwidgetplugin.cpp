// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "mockwidgetplugin.h"

#include <QLabel>

QString MockWidgetPlugin::cardType() const
{
    return "mock";
}

QString MockWidgetPlugin::displayName() const
{
    return "Mock Card";
}

QWidget* MockWidgetPlugin::createWidget(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QWidget* parent) const
{
    auto* label = new QLabel(groups.isEmpty() ? cardType() : groups.first().key, parent);
    label->setObjectName("mockLabel");
    return label;
}

QWidget* MockWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* shell = new QWidget(parent);
    shell->setObjectName("mockShell");
    return shell;
}

void MockWidgetPlugin::addGroup(const LibreSCRS::AgentClient::FieldGroup& group, QWidget* widget) const
{
    Q_UNUSED(widget);
    recordedKeys.append(group.key);
}

QStringList MockWidgetPlugin::recordedGroupKeys() const
{
    return recordedKeys;
}
