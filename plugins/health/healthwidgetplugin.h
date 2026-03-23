// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef HEALTHWIDGETPLUGIN_H
#define HEALTHWIDGETPLUGIN_H

#include "plugin/cardwidgetplugin.h"
#include <QObject>
#include <QtPlugin>

class HealthWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/1.0" FILE "rs-health.json")
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
    QWidget* createWidget(const plugin::CardData& data, QWidget* parent) const override;
    QWidget* createEmptyWidget(QWidget* parent) const override;
    void addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const override;
    bool supportsPrinting() const override
    {
        return true;
    }
    void print(const plugin::CardData& data, QPrinter* printer) const override;
};

#endif // HEALTHWIDGETPLUGIN_H
