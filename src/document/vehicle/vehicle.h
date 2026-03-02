// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef VEHICLE_H
#define VEHICLE_H

#include <QWidget>
#include <vehiclecard/vehicletypes.h>
#include "document/document.h"

class VehicleReader;

namespace Ui {
class Vehicle;
}

class Vehicle : public Document
{
    Q_OBJECT
public:
    explicit Vehicle(std::string reader, QWidget *parent = nullptr);
    ~Vehicle();

private slots:
    void vehicleDataReceived(const vehiclecard::VehicleDocumentData& data);

    void on_toolButton_clicked();

private:
    Ui::Vehicle *ui;

    vehiclecard::VehicleDocumentData vehicleData;

    using VehicleReaderUPtr = std::unique_ptr<VehicleReader>;
    VehicleReaderUPtr vehicleReader;
};

#endif // VEHICLE_H
