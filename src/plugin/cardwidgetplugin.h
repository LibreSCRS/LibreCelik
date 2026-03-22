// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CARDWIDGETPLUGIN_H
#define CARDWIDGETPLUGIN_H

#include <plugin/card_data.h>

#include <QIcon>
#include <QString>
#include <QStringList>
#include <QWidget>

class QPrinter;

class CardWidgetPlugin
{
public:
    virtual ~CardWidgetPlugin() = default;

    // Identity — cardType must match middleware CardPlugin::pluginId()
    virtual QString cardType() const = 0;
    virtual QString displayName() const = 0;
    virtual QIcon icon() const
    {
        return {};
    }
    virtual QStringList additionalCardTypes() const
    {
        return {};
    }

    // Main widget — receives CardData, returns populated QWidget.
    // Caller takes ownership of the returned widget.
    virtual QWidget* createWidget(const plugin::CardData& data, QWidget* parent) const = 0;

    // Progressive display: create an empty shell widget for incremental population.
    // Returns nullptr if plugin does not support streaming (fallback to createWidget).
    virtual QWidget* createEmptyWidget(QWidget* parent) const
    {
        Q_UNUSED(parent);
        return nullptr;
    }

    // Progressive display: add a group to an existing widget created by createEmptyWidget.
    virtual void addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
    {
        Q_UNUSED(group);
        Q_UNUSED(widget);
    }

    // PKI UI override — return nullptr to use default TokenSection.
    // Caller takes ownership if non-null.
    virtual QWidget* createPKIWidget(QWidget* /*parent*/) const
    {
        return nullptr;
    }

    // Printing — existing HTML/CSS templates stay temporarily inside plugins.
    virtual bool supportsPrinting() const
    {
        return false;
    }
    virtual void print(const plugin::CardData& data, QPrinter* printer) const
    {
        Q_UNUSED(data);
        Q_UNUSED(printer);
    }
};

Q_DECLARE_INTERFACE(CardWidgetPlugin, "org.librescrs.CardWidgetPlugin/1.0")

#endif // CARDWIDGETPLUGIN_H
