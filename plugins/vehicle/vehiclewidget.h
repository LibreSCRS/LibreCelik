// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <plugin/card_data.h>
#include <QWidget>

class CollapsibleSection;

class VehicleWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VehicleWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    const plugin::CardData& cardData() const
    {
        return data;
    }

private:
    void buildLayout();
    CollapsibleSection* buildEngineSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildMassSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildCapacitySection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildDocumentSection(const plugin::CardFieldGroup* group);
    CollapsibleSection* buildOwnerUserSection(const plugin::CardFieldGroup* ownerGroup,
                                              const plugin::CardFieldGroup* userGroup);

    plugin::CardData data;
};
