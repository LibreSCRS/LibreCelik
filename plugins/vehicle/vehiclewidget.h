// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef VEHICLEWIDGET_H
#define VEHICLEWIDGET_H

#include <plugin/card_data.h>
#include <QWidget>

namespace Ui {
class Vehicle;
}

class VehicleWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VehicleWidget(const plugin::CardData& data, QWidget* parent = nullptr);
    ~VehicleWidget();

    const plugin::CardData& cardData() const
    {
        return data;
    }

private:
    void populateVehicleData(const plugin::CardFieldGroup* group);
    void populateOwnerData(const plugin::CardFieldGroup* group);
    void populateUserData(const plugin::CardFieldGroup* group);

    Ui::Vehicle* ui;
    plugin::CardData data;
};

#endif // VEHICLEWIDGET_H
