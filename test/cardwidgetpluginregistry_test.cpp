// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include "plugin/cardwidgetpluginregistry.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QLabel>
#include <QList>

namespace {
int argc = 1;
char arg0[] = "test";
char* argv[] = {arg0, nullptr};
QApplication app(argc, argv);
} // namespace

TEST(CardWidgetPluginRegistryTest, RegistersTheLinkedInCardPage)
{
    CardWidgetPluginRegistry registry;
    registry.loadStaticPlugins();
    EXPECT_NE(registry.findByCardType(QStringLiteral("mock")), nullptr);
}

TEST(CardWidgetPluginRegistryTest, FindByCardType)
{
    CardWidgetPluginRegistry registry;
    registry.loadStaticPlugins();
    auto* plugin = registry.findByCardType("mock");
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin->cardType(), "mock");
    EXPECT_EQ(plugin->displayName(), "Mock Card");
}

TEST(CardWidgetPluginRegistryTest, FindByCardTypeNotFound)
{
    CardWidgetPluginRegistry registry;
    registry.loadStaticPlugins();
    EXPECT_EQ(registry.findByCardType("nonexistent"), nullptr);
}

TEST(CardWidgetPluginRegistryTest, CreateWidgetFromPlugin)
{
    CardWidgetPluginRegistry registry;
    registry.loadStaticPlugins();
    auto* plugin = registry.findByCardType("mock");
    ASSERT_NE(plugin, nullptr);

    LibreSCRS::AgentClient::FieldGroup group;
    group.key = QStringLiteral("mock");

    QWidget parent;
    auto* widget = plugin->createWidget(QList<LibreSCRS::AgentClient::FieldGroup>{group}, &parent);
    ASSERT_NE(widget, nullptr);
    auto* label = qobject_cast<QLabel*>(widget);
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text(), "mock");
}
