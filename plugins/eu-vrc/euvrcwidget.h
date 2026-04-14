// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <plugin/card_data.h>

namespace LibreSCRS {
class CardHeaderCard;
}

class CollapsibleSection;
class QToolButton;
class QVBoxLayout;

class EuVrcWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    // Full constructor — builds entire widget from complete CardData
    explicit EuVrcWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    // Empty shell constructor — for progressive display
    explicit EuVrcWidget(QWidget* parent);

    // Progressive display: add a group incrementally
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
    void buildShell();
    void addRegistrationGroup(const plugin::CardFieldGroup& group);
    void addVehicleGroup(const plugin::CardFieldGroup& group);
    void addHolderGroup(const plugin::CardFieldGroup& group);
    void addUserGroup(const plugin::CardFieldGroup& group);
    void addNationalGroup(const plugin::CardFieldGroup& group);
    void addOwnerGroup(const plugin::CardFieldGroup& group);
    CollapsibleSection* buildRegistrationSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildVehicleSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildEngineTechnicalSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildHolderSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildUserSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildNationalSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildOwnerSection(const plugin::CardFieldGroup* group);

    plugin::CardData data;
    QVBoxLayout* contentLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QToolButton* printBtn = nullptr;
    LibreSCRS::CardHeaderCard* headerCard = nullptr;
};
