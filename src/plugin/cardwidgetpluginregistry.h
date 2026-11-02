// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "cardwidgetplugin.h"

#include <QList>
#include <QMap>
#include <QString>

/// @brief Registers the card pages linked into this binary and looks them up
/// by the card type they declare.
class CardWidgetPluginRegistry
{
public:
    CardWidgetPluginRegistry() = default;

    CardWidgetPluginRegistry(const CardWidgetPluginRegistry&) = delete;
    CardWidgetPluginRegistry& operator=(const CardWidgetPluginRegistry&) = delete;

    /// Register every card page linked into this binary (the static Qt
    /// plugins its build imported) that implements `CardWidgetPlugin`.
    void loadStaticPlugins();
    CardWidgetPlugin* findByCardType(const QString& cardType) const;
    QList<CardWidgetPlugin*> plugins() const;

private:
    QMap<QString, CardWidgetPlugin*> pluginMap;
};
