// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "plugin/cardwidgetplugin.h"

#include <QObject>
#include <QtPlugin>

class PIVWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/1.0" FILE "piv.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override
    {
        return QStringLiteral("piv");
    }
    QString displayName() const override
    {
        return qtTrId("lc-piv-widget-title");
    }
    QWidget* createWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent) const override;
    QWidget* createEmptyWidget(QWidget* parent) const override;
    void addGroup(const LibreSCRS::Plugin::CardFieldGroup& group, QWidget* widget) const override;
    bool supportsPrinting() const override
    {
        return true;
    }
    void print(const LibreSCRS::Plugin::CardData& data) const override;
};
