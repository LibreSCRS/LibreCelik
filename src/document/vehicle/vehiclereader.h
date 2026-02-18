// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef VEHICLEREADER_H
#define VEHICLEREADER_H

#include <future>
#include <memory>
#include <string>
#include <QObject>
#include <vehiclecard/vehicletypes.h>

namespace vehiclecard {
class VehicleCard;
}

class VehicleReader : public QObject
{
    Q_OBJECT
public:
    VehicleReader(const std::string& cardReader, QObject *parent = nullptr);
    ~VehicleReader();

    void requestData();

signals:
    void vehicleDataRead(vehiclecard::VehicleDocumentData vehicleData);

    void readingStarted();
    void readingFinished();

protected:
    std::unique_ptr<vehiclecard::VehicleCard> initVehicleCard();
    vehiclecard::VehicleDocumentData readVehicleData(std::unique_ptr<vehiclecard::VehicleCard>& card);

private:
    std::string cardReader;
    std::future<void> futureData;

    static std::mutex cardAccessMutex;
    static std::condition_variable cv;
    static bool processing;
};

#endif // VEHICLEREADER_H
