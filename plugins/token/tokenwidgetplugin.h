// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include "plugin/cardwidgetplugin.h"

#include <QObject>
#include <QtPlugin>

class TokenWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/1.0" FILE "token.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override
    {
        return QStringLiteral("token");
    }
    QString displayName() const override
    {
        return qtTrId("lc-token-widget-title");
    }
    QStringList additionalCardTypes() const override
    {
        return {QStringLiteral("pkcs15"), QStringLiteral("cardedge")};
    }
    QWidget* createWidget(const plugin::CardData& data, QWidget* parent) const override;
};
