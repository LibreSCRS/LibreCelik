// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef NDEBUG
#include <sstream>
#include <thread>
#endif

#include "utils/libreceliklog.h"
#include <vehiclecard/vehiclecard.h>
#include "vehiclereader.h"

std::mutex VehicleReader::cardAccessMutex;
std::condition_variable VehicleReader::cv;
bool VehicleReader::processing = false;

VehicleReader::VehicleReader(const std::string& cardReader, QObject *parent) : cardReader(cardReader), QObject(parent)
{
    qRegisterMetaType<vehiclecard::VehicleDocumentData>();
}

VehicleReader::~VehicleReader()
{
}

void VehicleReader::requestData()
{
    futureData = std::async(std::launch::async, [this]() {
        std::unique_lock<std::mutex> lock (cardAccessMutex);
        cv.wait(lock, [this](){return processing == false;});
        processing = true;

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

        processing = false;
        lock.unlock();
        cv.notify_one();
    });
}

std::unique_ptr<vehiclecard::VehicleCard> VehicleReader::initVehicleCard()
{
    std::unique_ptr<vehiclecard::VehicleCard> card = nullptr;
    try
    {
        card = std::make_unique<vehiclecard::VehicleCard>(cardReader);
    }
    catch (std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not init vehicle card on reader: " << cardReader << ". Exception: " << re.what();
    }
    return card;
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
