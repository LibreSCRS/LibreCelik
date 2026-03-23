// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include "cardwidgetplugin.h"

#include <QList>
#include <QMap>
#include <QPluginLoader>
#include <QString>

class CardWidgetPluginRegistry
{
public:
    CardWidgetPluginRegistry() = default;
    ~CardWidgetPluginRegistry();

    CardWidgetPluginRegistry(const CardWidgetPluginRegistry&) = delete;
    CardWidgetPluginRegistry& operator=(const CardWidgetPluginRegistry&) = delete;

    void loadPluginsFromDirectory(const QString& dir);
    CardWidgetPlugin* findByCardType(const QString& cardType) const;
    QList<CardWidgetPlugin*> plugins() const;

private:
    QList<QPluginLoader*> loaders;
    QMap<QString, CardWidgetPlugin*> pluginMap;
};
