// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "cardwidgetplugin.h"

#include <QList>
#include <QMap>
#include <QPluginLoader>
#include <QString>

/// @brief Loads CardWidgetPlugin shared modules from a directory and looks
/// them up by the card type they declare.
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
