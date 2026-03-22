// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef RSEIDWIDGETPLUGIN_H
#define RSEIDWIDGETPLUGIN_H

#include "plugin/cardwidgetplugin.h"
#include <QObject>
#include <QtPlugin>

class RsEidWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/1.0" FILE "rs-eid.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override
    {
        return QStringLiteral("rs-eid");
    }
    QString displayName() const override
    {
        return qtTrId("lc-eid-title-serbian");
    }
    QWidget* createWidget(const plugin::CardData& data, QWidget* parent) const override;
    QWidget* createEmptyWidget(QWidget* parent) const override;
    void addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const override;
};

#endif // RSEIDWIDGETPLUGIN_H
