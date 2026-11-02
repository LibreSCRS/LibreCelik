// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "cardwidgetpluginregistry.h"

#include "utils/libreceliklog.h"

#include <QJsonObject>
#include <QPluginLoader>

void CardWidgetPluginRegistry::loadStaticPlugins()
{
    for (const QStaticPlugin& staticPlugin : QPluginLoader::staticPlugins()) {
        // Taken from the interface declaration rather than re-spelled here: a
        // second copy of the IID is a copy that can disagree with the one the
        // plugins are built against, and the disagreement shows up only as
        // card pages silently missing.
        const QString iid = staticPlugin.metaData().value(QStringLiteral("IID")).toString();
        if (iid != QLatin1StringView(qobject_interface_iid<CardWidgetPlugin*>()))
            continue;
        auto* plugin = qobject_cast<CardWidgetPlugin*>(staticPlugin.instance());
        if (plugin == nullptr) {
            qCWarning(lcPluginRegistry) << "Static plugin with the card-page IID does not implement CardWidgetPlugin";
            continue;
        }
        qCInfo(lcPluginRegistry) << "Card page:" << plugin->cardType() << "(" << plugin->displayName() << ")";
        pluginMap.insert(plugin->cardType(), plugin);
        for (const auto& type : plugin->additionalCardTypes()) {
            qCInfo(lcPluginRegistry) << "  also registered for cardType:" << type;
            pluginMap.insert(type, plugin);
        }
    }
}

CardWidgetPlugin* CardWidgetPluginRegistry::findByCardType(const QString& cardType) const
{
    return pluginMap.value(cardType, nullptr);
}

QList<CardWidgetPlugin*> CardWidgetPluginRegistry::plugins() const
{
    return pluginMap.values();
}
