// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <chrono>
#include <thread>

#ifndef NDEBUG
#include <sstream>
#endif

#include "utils/libreceliklog.h"
#include <healthcard/healthcard.h>
#include "healthreader.h"

HealthReader::HealthReader(const std::string& cardReader, QObject *parent)
    : cardReader(cardReader), QObject(parent)
{
    qRegisterMetaType<healthcard::HealthDocumentData>();
    qRegisterMetaType<eidcard::CertificateList>();
}

HealthReader::~HealthReader()
{
    stopRequested.store(true);
    if (futureData.valid())
        futureData.wait();
    if (futureCerts.valid())
        futureCerts.wait();
    if (futurePinData.valid())
        futurePinData.wait();
}

void HealthReader::requestData()
{
    futureData = std::async(std::launch::async, [this]() {
        emit readingStarted();

#ifndef NDEBUG
        std::stringstream ss;
        ss << std::this_thread::get_id();
        qDebug(libreCelikHealth) << "Health requestData: " << cardReader << ". Thread: " << ss.str();
#endif

        auto card = initHealthCard();
        if (card) {
            auto data = readHealthData(card);
            emit healthDataRead(data);
        } else {
            qCWarning(libreCelikHealth) << "Cannot initialize health card on: " << cardReader;
        }

        emit readingFinished();
    });
}

void HealthReader::requestCertificates()
{
    futureCerts = std::async(std::launch::async, [this]() {
        auto card = initHealthCard();
        if (!card) {
            qCWarning(libreCelikHealth) << "Cannot initialize health card for certificates on: " << cardReader;
            return;
        }
        try {
            auto certs = card->readCertificates();
            if (!certs.empty())
                emit certificateDataRead(std::move(certs));
        } catch (const std::exception& e) {
            qCWarning(libreCelikHealth) << "Cannot read certificates on: " << cardReader
                                        << ". Exception: " << e.what();
        }
    });
}

void HealthReader::requestPINTriesLeft()
{
    futurePinData = std::async(std::launch::async, [this]() {
        auto card = initHealthCard();
        if (!card) {
            emit pinTriesLeftRead(-1, false);
            return;
        }
        try {
            auto result = card->getPINTriesLeft();
            emit pinTriesLeftRead(result.retriesLeft, result.blocked);
        } catch (const std::exception& e) {
            qCWarning(libreCelikHealth) << "Cannot get PIN tries on: " << cardReader
                                        << ". Exception: " << e.what();
            emit pinTriesLeftRead(-1, false);
        }
    });
}

void HealthReader::requestChangePIN(const QString& oldPin, const QString& newPin)
{
    futurePinData = std::async(std::launch::async, [this, oldPin, newPin]() {
        auto card = initHealthCard();
        if (!card) {
            emit pinChangeFailed(-1, false, qtTrId("lc-eidreader-failed-connect"));
            return;
        }
        try {
            auto result = card->changePIN(oldPin.toStdString(), newPin.toStdString());
            if (result.success) {
                emit pinChangeSuccess();
            } else {
                QString msg;
                if (result.blocked) {
                    msg = qtTrId("lc-eidreader-pin-blocked");
                } else if (result.retriesLeft >= 0) {
                    msg = qtTrId("lc-eidreader-pin-incorrect").arg(result.retriesLeft);
                } else {
                    msg = qtTrId("lc-eidreader-pin-change-failed");
                }
                emit pinChangeFailed(result.retriesLeft, result.blocked, msg);
            }
        } catch (const std::exception& e) {
            qCWarning(libreCelikHealth) << "PIN change failed on: " << cardReader
                                        << ". Exception: " << e.what();
            emit pinChangeFailed(-1, false, qtTrId("lc-eidreader-pin-change-exception").arg(QString::fromStdString(e.what())));
        }
    });
}

std::unique_ptr<healthcard::HealthCard> HealthReader::initHealthCard()
{
    for (int attempt = 0; attempt < 3; attempt++) {
        if (stopRequested.load())
            return nullptr;
        try {
            return std::make_unique<healthcard::HealthCard>(cardReader);
        } catch (std::runtime_error& re) {
            if (attempt < 2) {
                for (int i = 0; i < 5; i++) {
                    if (stopRequested.load()) return nullptr;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } else {
                qCWarning(libreCelikHealth) << "Cannot init health card on reader: " << cardReader
                                            << ". Exception: " << re.what();
            }
        }
    }
    return nullptr;
}

healthcard::HealthDocumentData HealthReader::readHealthData(std::unique_ptr<healthcard::HealthCard>& card)
{
    healthcard::HealthDocumentData data;
    if (!card)
        return data;
    try {
        data = card->readDocumentData();
    } catch (std::runtime_error& re) {
        qCWarning(libreCelikHealth) << "Cannot read health data on reader: " << cardReader
                                    << ". Exception: " << re.what();
    }
    return data;
}
