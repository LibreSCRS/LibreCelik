// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/Plugin/CardData.h>

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
    explicit EuVrcWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent = nullptr);

    // Empty shell constructor — for progressive display
    explicit EuVrcWidget(QWidget* parent);

    // Progressive display: add a group incrementally
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
    void buildShell();
    void addRegistrationGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addVehicleGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addHolderGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addUserGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addNationalGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addOwnerGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    CollapsibleSection* buildRegistrationSection(const LibreSCRS::Plugin::CardFieldGroup* group);
    CollapsibleSection* buildVehicleSection(const LibreSCRS::Plugin::CardFieldGroup* group);
    CollapsibleSection* buildEngineTechnicalSection(const LibreSCRS::Plugin::CardFieldGroup* group);
    CollapsibleSection* buildHolderSection(const LibreSCRS::Plugin::CardFieldGroup* group);
    CollapsibleSection* buildUserSection(const LibreSCRS::Plugin::CardFieldGroup* group);
    CollapsibleSection* buildNationalSection(const LibreSCRS::Plugin::CardFieldGroup* group);
    CollapsibleSection* buildOwnerSection(const LibreSCRS::Plugin::CardFieldGroup* group);

    LibreSCRS::Plugin::CardData data;
    QVBoxLayout* outerLayout = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QToolButton* printBtn = nullptr;
    LibreSCRS::CardHeaderCard* headerCard = nullptr;
};
