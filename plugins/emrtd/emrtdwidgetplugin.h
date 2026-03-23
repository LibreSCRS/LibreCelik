// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include "plugin/cardwidgetplugin.h"

#include <QObject>
#include <QtPlugin>

class EMRTDWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/1.0" FILE "emrtd.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override
    {
        return QStringLiteral("emrtd");
    }
    QString displayName() const override
    {
        return QStringLiteral("eMRTD / Passport");
    }
    QStringList additionalCardTypes() const override
    {
        return {QStringLiteral("pkcs15")};
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
