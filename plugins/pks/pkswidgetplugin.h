// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef PKSWIDGETPLUGIN_H
#define PKSWIDGETPLUGIN_H

#include "plugin/cardwidgetplugin.h"
#include <QObject>
#include <QtPlugin>

class PksWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/1.0" FILE "rs-pks.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override
    {
        return QStringLiteral("rs-pks");
    }
    QString displayName() const override
    {
        return qtTrId("lc-pks-title");
    }
    QWidget* createWidget(const plugin::CardData& data, QWidget* parent) const override;
    QWidget* createEmptyWidget(QWidget* parent) const override;
    void addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const override;
};

#endif // PKSWIDGETPLUGIN_H
