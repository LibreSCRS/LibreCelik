// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef VEHICLEWIDGETPLUGIN_H
#define VEHICLEWIDGETPLUGIN_H

#include "plugin/cardwidgetplugin.h"
#include <QObject>
#include <QtPlugin>

class VehicleWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/1.0" FILE "vehicle.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override
    {
        return QStringLiteral("vehicle");
    }
    QString displayName() const override
    {
        return qtTrId("lc-vehicle-title");
    }
    QWidget* createWidget(const plugin::CardData& data, QWidget* parent) const override;
    QWidget* createEmptyWidget(QWidget* parent) const override;
    void addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const override;
};

#endif // VEHICLEWIDGETPLUGIN_H
