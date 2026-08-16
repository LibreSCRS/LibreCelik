// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "plugin/cardwidgetplugin.h"
#include <QObject>
#include <QtPlugin>

class RsHealthWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/2.0" FILE "rs-health.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override
    {
        return QStringLiteral("rs-health");
    }
    QString displayName() const override
    {
        return qtTrId("lc-health-title");
    }
    QWidget* createWidget(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QWidget* parent) const override;
    QWidget* createEmptyWidget(QWidget* parent) const override;
    void addGroup(const LibreSCRS::AgentClient::FieldGroup& group, QWidget* widget) const override;
    bool supportsPrinting() const override
    {
        return true;
    }
    void print(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const override;
};
