// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "cardwidgetpluginregistry.h"

#include "utils/libreceliklog.h"

#include <QDir>
#include <QJsonObject>

CardWidgetPluginRegistry::~CardWidgetPluginRegistry()
{
    pluginMap.clear();
    qDeleteAll(loaders);
}

void CardWidgetPluginRegistry::loadPluginsFromDirectory(const QString& dir)
{
    QDir pluginDir(dir);
    if (!pluginDir.exists())
        return;

    for (const auto& fileName : pluginDir.entryList(QDir::Files)) {
        auto path = pluginDir.absoluteFilePath(fileName);
        auto* loader = new QPluginLoader(path);

        auto metaData = loader->metaData();
        auto iid = metaData.value("IID").toString();
        // Taken from the interface declaration rather than re-spelled here: a
        // second copy of the IID is a copy that can disagree with the one the
        // plugins are built against, and the disagreement shows up only as
        // plugins silently not loading.
        if (iid != QLatin1StringView(qobject_interface_iid<CardWidgetPlugin*>())) {
            delete loader;
            continue;
        }

        QObject* instance = loader->instance();
        if (!instance) {
            qCWarning(lcPluginRegistry) << "Failed to load plugin" << path << ":" << loader->errorString();
            delete loader;
            continue;
        }

        auto* plugin = qobject_cast<CardWidgetPlugin*>(instance);
        if (!plugin) {
            qCWarning(lcPluginRegistry) << "Plugin" << path << "does not implement CardWidgetPlugin";
            loader->unload();
            delete loader;
            continue;
        }

        qCInfo(lcPluginRegistry) << "Loaded GUI plugin:" << plugin->cardType() << "(" << plugin->displayName() << ")";
        pluginMap.insert(plugin->cardType(), plugin);
        for (const auto& type : plugin->additionalCardTypes()) {
            qCInfo(lcPluginRegistry) << "  also registered for cardType:" << type;
            pluginMap.insert(type, plugin);
        }
        loaders.append(loader);
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
