// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <plugin/card_data.h>
#include <QWidget>

class CollapsibleSection;
class QToolButton;
class QVBoxLayout;

class VehicleWidget : public QWidget
{
    Q_OBJECT
public:
    // Full constructor — builds entire widget from complete CardData (unchanged)
    explicit VehicleWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    // Empty shell constructor — for progressive display
    explicit VehicleWidget(QWidget* parent);

    // Progressive display: add a group incrementally
    void addGroup(const plugin::CardFieldGroup& group);

    const plugin::CardData& cardData() const
    {
        return data;
    }

    Q_INVOKABLE void enablePrintButton();

signals:
    void printRequested(const plugin::CardData& data);

private:
    void buildLayout();
    void buildShell();
    void addVehicleGroup(const plugin::CardFieldGroup& group);
    void addOwnerGroup(const plugin::CardFieldGroup& group);
    void addUserGroup(const plugin::CardFieldGroup& group);
    CollapsibleSection* buildEngineSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildMassSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildCapacitySection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildDocumentSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildOwnerUserSection(const plugin::CardFieldGroup* ownerGroup,
                                              const plugin::CardFieldGroup* userGroup);

    plugin::CardData data;
    QVBoxLayout* contentLayout = nullptr; // inner layout of the outer CollapsibleSection
    CollapsibleSection* outerSection = nullptr;
    QToolButton* printBtn = nullptr;
};
