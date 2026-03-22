// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef HEALTHWIDGET_H
#define HEALTHWIDGET_H

#include <plugin/card_data.h>
#include <QWidget>

class CollapsibleSection;
class QVBoxLayout;

class HealthWidget : public QWidget
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

private:
    void buildLayout();
    void buildEmptyShell();
    void transformPermanentlyValid();
    void transformPermanentlyValid(plugin::CardFieldGroup& group);

    void addPersonalGroup(const plugin::CardFieldGroup& group);
    void addInsuranceGroup(const plugin::CardFieldGroup& group);
    void addAddressGroup(const plugin::CardFieldGroup& group);
    void addCarrierGroup(const plugin::CardFieldGroup& group);
    void addTaxpayerGroup(const plugin::CardFieldGroup& group);

    plugin::CardData data;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    CollapsibleSection* carrierSection = nullptr;
};

#endif // HEALTHWIDGET_H
