// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <plugin/card_data.h>

class CollapsibleSection;
class QToolButton;
class QVBoxLayout;

class HealthWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    // Full constructor — builds entire widget at once (existing behaviour)
    explicit HealthWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    // Empty constructor — creates outer shell for progressive population via addGroup()
    explicit HealthWidget(QWidget* parent);

    // Progressive display: append a group's UI section to the widget
    void addGroup(const plugin::CardFieldGroup& group);

    const plugin::CardData& cardData() const
    {
        return data;
    }

    Q_INVOKABLE void enablePrintButton();

signals:
    void printRequested(const plugin::CardData& data);

protected:
    void retranslateUi() override;

private:
    void buildEmptyShell();
    void transformPermanentlyValid(plugin::CardFieldGroup& group);

    void addPersonalGroup(const plugin::CardFieldGroup& group);
    void addInsuranceGroup(const plugin::CardFieldGroup& group);
    void addAddressGroup(const plugin::CardFieldGroup& group);
    void addCarrierGroup(const plugin::CardFieldGroup& group);
    void addTaxpayerGroup(const plugin::CardFieldGroup& group);

    plugin::CardData data;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    QToolButton* printBtn = nullptr;
};
