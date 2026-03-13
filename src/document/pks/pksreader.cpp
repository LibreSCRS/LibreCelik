// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "pksreader.h"
#include "utils/libreceliklog.h"
#include <pkscard/pkscard.h>

PKSReader::PKSReader(const std::string& cardReader, QObject* parent) : cardReader(cardReader), QObject(parent)
{
    qRegisterMetaType<eidcard::CertificateList>();
}

PKSReader::~PKSReader()
{
    if (futureCerts.valid())
        futureCerts.wait();
    if (futurePinData.valid())
        futurePinData.wait();
}

void PKSReader::requestCertificates()
{
    futureCerts = std::async(std::launch::async, [this]() {
        emit readingStarted();
        try {
            pkscard::PKSCard card(cardReader);
            auto certs = card.readCertificates();
            if (!certs.empty())
                emit certificateDataRead(std::move(certs));
        } catch (const std::exception& e) {
            qCWarning(libreCelikPks) << "Cannot read certificates on: " << cardReader << ". Exception: " << e.what();
        }
        emit readingFinished();
    });
}

void PKSReader::requestPINTriesLeft()
{
    futurePinData = std::async(std::launch::async, [this]() {
        try {
            pkscard::PKSCard card(cardReader);
            auto result = card.getPINTriesLeft();
            emit pinTriesLeftRead(result.retriesLeft, result.blocked);
        } catch (const std::exception& e) {
            qCWarning(libreCelikPks) << "Cannot get PIN tries on: " << cardReader << ". Exception: " << e.what();
            emit pinTriesLeftRead(-1, false);
        }
    });
}

void PKSReader::requestChangePIN(const QString& oldPin, const QString& newPin)
{
    futurePinData = std::async(std::launch::async, [this, oldPin, newPin]() {
        try {
            pkscard::PKSCard card(cardReader);
            auto result = card.changePIN(oldPin.toStdString(), newPin.toStdString());
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
            qCWarning(libreCelikPks) << "PIN change failed on: " << cardReader << ". Exception: " << e.what();
            emit pinChangeFailed(-1, false,
                                 qtTrId("lc-eidreader-pin-change-exception").arg(QString::fromStdString(e.what())));
        }
    });
}
