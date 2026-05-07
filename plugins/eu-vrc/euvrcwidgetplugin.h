// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "plugin/cardwidgetplugin.h"
#include <QObject>
#include <QtPlugin>

class EuVrcWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/1.0" FILE "eu-vrc.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override
    {
        return QStringLiteral("eu-vrc");
    }
    QString displayName() const override
    {
        return qtTrId("lc-euvrc-title");
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
