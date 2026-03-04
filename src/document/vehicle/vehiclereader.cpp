// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <chrono>
#include <thread>

#ifndef NDEBUG
#include <sstream>
#endif

#include "utils/libreceliklog.h"
#include <vehiclecard/vehiclecard.h>
#include "vehiclereader.h"

VehicleReader::VehicleReader(const std::string& cardReader, QObject *parent) : cardReader(cardReader), QObject(parent)
{
    qRegisterMetaType<vehiclecard::VehicleDocumentData>();
}

VehicleReader::~VehicleReader()
{
    stopRequested.store(true);
    if (futureData.valid())
        futureData.wait();
}

void VehicleReader::requestData()
{
    futureData = std::async(std::launch::async, [this]() {
        emit readingStarted();

#ifndef NDEBUG
        std::stringstream ss;
        ss << std::this_thread::get_id();

        qDebug(libreCelikAPI) << "Vehicle requestData: " << cardReader << ". Thread: " <<  ss.str();
#endif

        auto vehicleCard = initVehicleCard();
        if (vehicleCard != nullptr)
        {
            auto data = readVehicleData(vehicleCard);
            emit vehicleDataRead(data);
        }
        else
        {
            qDebug(libreCelikAPI) << "Can not initialize vehicle card session on: " << cardReader;
        }

        emit readingFinished();
    });
}

std::unique_ptr<vehiclecard::VehicleCard> VehicleReader::initVehicleCard()
{
    for (int attempt = 0; attempt < 3; attempt++)
    {
        if (stopRequested.load())
            return nullptr;
        try
        {
            return std::make_unique<vehiclecard::VehicleCard>(cardReader);
        }
        catch (std::runtime_error& re)
        {
            if (attempt < 2) {
                for (int i = 0; i < 5; i++) {
                    if (stopRequested.load()) return nullptr;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } else {
                qCWarning(libreCelikAPI) << "Can not init vehicle card on reader: " << cardReader << ". Exception: " << re.what();
            }
        }
    }
    return nullptr;
}

vehiclecard::VehicleDocumentData VehicleReader::readVehicleData(std::unique_ptr<vehiclecard::VehicleCard>& card)
{
    vehiclecard::VehicleDocumentData data;
    if (!card)
        return data;

    try
    {
        data = card->readDocumentData();
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read vehicle data on reader: " << cardReader << ". Exception: " << re.what();
    }
    return data;
}
