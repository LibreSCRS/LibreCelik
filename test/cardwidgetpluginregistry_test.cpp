// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include "plugin/cardwidgetpluginregistry.h"

#include <QApplication>
#include <QDir>
#include <QLabel>

namespace {
int argc = 1;
char arg0[] = "test";
char* argv[] = {arg0, nullptr};
QApplication app(argc, argv);
} // namespace

TEST(CardWidgetPluginRegistryTest, LoadsPluginFromDirectory)
{
    CardWidgetPluginRegistry registry;
    registry.loadPluginsFromDirectory(QString(MOCK_WIDGET_PLUGIN_DIR));
    EXPECT_EQ(registry.plugins().size(), 1);
}

TEST(CardWidgetPluginRegistryTest, FindByCardType)
{
    CardWidgetPluginRegistry registry;
    registry.loadPluginsFromDirectory(QString(MOCK_WIDGET_PLUGIN_DIR));
    auto* plugin = registry.findByCardType("mock");
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin->cardType(), "mock");
    EXPECT_EQ(plugin->displayName(), "Mock Card");
}

TEST(CardWidgetPluginRegistryTest, FindByCardTypeNotFound)
{
    CardWidgetPluginRegistry registry;
    registry.loadPluginsFromDirectory(QString(MOCK_WIDGET_PLUGIN_DIR));
    EXPECT_EQ(registry.findByCardType("nonexistent"), nullptr);
}

TEST(CardWidgetPluginRegistryTest, CreateWidgetFromPlugin)
{
    CardWidgetPluginRegistry registry;
    registry.loadPluginsFromDirectory(QString(MOCK_WIDGET_PLUGIN_DIR));
    auto* plugin = registry.findByCardType("mock");
    ASSERT_NE(plugin, nullptr);

    plugin::CardData data;
    data.cardType = "mock";

    QWidget parent;
    auto* widget = plugin->createWidget(data, &parent);
    ASSERT_NE(widget, nullptr);
    auto* label = qobject_cast<QLabel*>(widget);
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text(), "mock");
}

TEST(CardWidgetPluginRegistryTest, EmptyDirectoryLoadsNothing)
{
    CardWidgetPluginRegistry registry;
    QDir tmpDir(QDir::tempPath() + "/librescrs-test-empty-plugins");
    tmpDir.mkpath(".");
    registry.loadPluginsFromDirectory(tmpDir.absolutePath());
    EXPECT_EQ(registry.plugins().size(), 0);
    tmpDir.removeRecursively();
}
