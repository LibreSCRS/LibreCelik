// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/Plugin/CardData.h>

class CollapsibleSection;
class QToolButton;
class QVBoxLayout;

class HealthWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    // Full constructor — builds entire widget at once (existing behaviour)
    explicit HealthWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent = nullptr);

    // Empty constructor — creates outer shell for progressive population via addGroup()
    explicit HealthWidget(QWidget* parent);

    // Progressive display: append a group's UI section to the widget
    void addGroup(const LibreSCRS::Plugin::CardFieldGroup& group);

    const LibreSCRS::Plugin::CardData& cardData() const
    {
        return data;
    }

    Q_INVOKABLE void enablePrintButton();

signals:
    void printRequested(const LibreSCRS::Plugin::CardData& data);

protected:
    void retranslateUi() override;

private:
    void buildEmptyShell();
    void transformPermanentlyValid(LibreSCRS::Plugin::CardFieldGroup& group);

    void addPersonalGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addInsuranceGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addAddressGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addCarrierGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addTaxpayerGroup(const LibreSCRS::Plugin::CardFieldGroup& group);

    LibreSCRS::Plugin::CardData data;
    // Raw, untransformed groups as received from the plugin. Used by
    // retranslateUi() to rebuild from the original source-of-truth (the
    // visible CardData stored in `data` is mutated by
    // transformPermanentlyValid, so reusing it across language switches
    // would not yield the new "Yes"/"No" translation).
    std::vector<LibreSCRS::Plugin::CardFieldGroup> rawGroups;
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    QToolButton* printBtn = nullptr;
};
