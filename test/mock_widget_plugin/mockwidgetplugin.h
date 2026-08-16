// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include "plugin/cardwidgetplugin.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QWidget>

// Declared in a header rather than wholly inside the .cpp so a test target can
// include and instantiate the plugin directly instead of going through plugin
// loading.
class MockWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/2.0" FILE "mock-widget.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override;
    QString displayName() const override;

    QWidget* createWidget(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QWidget* parent) const override;

    // Streaming shell. The base default returns nullptr (meaning "this plugin
    // does not stream"), which would make a dispatch test's shell round-trip
    // vacuous — addGroup ignores the widget, so a null shell would still pass.
    // A real widget keeps the test honest about what the host hands over.
    QWidget* createEmptyWidget(QWidget* parent) const override;

    void addGroup(const LibreSCRS::AgentClient::FieldGroup& group, QWidget* widget) const override;

    // Every addGroup call appends the group's key here, in arrival order, so a
    // dispatch test can assert what the host actually handed the plugin rather
    // than what it meant to.
    [[nodiscard]] QStringList recordedGroupKeys() const;

private:
    // Mutable because the interface's dispatch entry point is const: the
    // recorder observes calls, it does not change what the plugin renders.
    mutable QStringList recordedKeys;
};
