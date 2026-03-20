// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef HEALTHWIDGET_H
#define HEALTHWIDGET_H

#include <plugin/card_data.h>
#include <QWidget>

namespace Ui {
class Health;
}

class HealthWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HealthWidget(const plugin::CardData& data, QWidget* parent = nullptr);
    ~HealthWidget();

    const plugin::CardData& cardData() const
    {
        return data;
    }

private:
    void populatePersonalData(const plugin::CardFieldGroup* group);
    void populateInsuranceData(const plugin::CardFieldGroup* group);
    void populateAddressData(const plugin::CardFieldGroup* group);
    void populateCarrierData(const plugin::CardFieldGroup* group);
    void populateTaxpayerData(const plugin::CardFieldGroup* group);

    Ui::Health* ui;
    plugin::CardData data;
};

#endif // HEALTHWIDGET_H
