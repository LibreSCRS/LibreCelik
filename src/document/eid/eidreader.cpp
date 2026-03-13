// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef NDEBUG
#include <sstream>
#include <thread>
#endif

#include "utils/libreceliklog.h"
#include <eidcard/eidcard.h>
#include "eidreader.h"
#include "config.h"
#include <QDir>
#include <QFile>

EIdReader::EIdReader(const std::string& cardReader, QObject* parent) : cardReader(cardReader), QObject(parent)
{
    qRegisterMetaType<eidcard::CardType>();
    qRegisterMetaType<eidcard::DocumentData>();
    qRegisterMetaType<eidcard::FixedPersonalData>();
    qRegisterMetaType<eidcard::VariablePersonalData>();
    qRegisterMetaType<eidcard::PhotoData>();
    qRegisterMetaType<eidcard::VerificationResult>();
    qRegisterMetaType<eidcard::CertificateList>();
}

EIdReader::~EIdReader()
{
    stopRequested.store(true);
    if (futureData.valid())
        futureData.wait();
    if (futurePinData.valid())
        futurePinData.wait();
}

void EIdReader::requestData()
{
    futureData = std::async(std::launch::async, [this]() {
        emit readingStarted();

#ifndef NDEBUG
        std::stringstream ss;
        ss << std::this_thread::get_id();

        qDebug(libreCelikAPI) << "EId requestData: " << cardReader << ". Thread: " << ss.str();
#endif

        if (!eidCard)
            eidCard = initEIdCard();

        if (eidCard != nullptr) {
            requestEIdData(eidCard);
            requestPhoto(eidCard);
            requestVerification(eidCard, LibreSCRS::VerificationOption::CheckCard |
                                             LibreSCRS::VerificationOption::CheckSignature);
        } else {
            qDebug(libreCelikAPI) << "Can not initialize eID card session on: " << cardReader;
        }

        emit readingFinished();
    });
}

std::unique_ptr<eidcard::EIdCard> EIdReader::initEIdCard()
{
    // Retry with delay — the card may not be ready immediately after a swap
    for (int attempt = 0; attempt < 3; attempt++) {
        if (stopRequested.load())
            return nullptr;
        try {
            return std::make_unique<eidcard::EIdCard>(cardReader);
        } catch (std::runtime_error& re) {
            if (attempt < 2) {
                for (int i = 0; i < 5; i++) {
                    if (stopRequested.load())
                        return nullptr;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } else {
                qCWarning(libreCelikAPI) << "Can not init eID card on reader: " << cardReader
                                         << ". Exception: " << re.what();
            }
        }
    }
    return nullptr;
}

void EIdReader::requestEIdData(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    if (!eidCard)
        return;

    auto cardType = eidCard->getCardType();
    emit cardTypeRead(cardType);

    try {
        emit fixedPersonalDataRead(eidCard->readFixedPersonalData());
    } catch (std::runtime_error& re) {
        qCWarning(libreCelikAPI) << "Can not read fixed personal data on reader: " << cardReader
                                 << ". Exception: " << re.what();
    }

    try {
        emit variablePersonalDataRead(eidCard->readVariablePersonalData());
    } catch (std::runtime_error& re) {
        qCWarning(libreCelikAPI) << "Can not read variable personal data on reader: " << cardReader
                                 << ". Exception: " << re.what();
    }

    try {
        emit documentDataRead(eidCard->readDocumentData());
    } catch (std::runtime_error& re) {
        qCWarning(libreCelikAPI) << "Can not read document data on reader: " << cardReader
                                 << ". Exception: " << re.what();
    }

    try {
        auto certs = eidCard->readCertificates();
        if (!certs.empty())
            emit certificateDataRead(std::move(certs));
    } catch (std::runtime_error& re) {
        qCWarning(libreCelikAPI) << "Can not read certificates on reader: " << cardReader
                                 << ". Exception: " << re.what();
    }

    if (cardType == eidcard::CardType::Gemalto2014 || cardType == eidcard::CardType::ForeignerIF2020) {
        try {
            auto result = eidCard->getPINTriesLeft();
            emit pinTriesLeftRead(result.retriesLeft, result.blocked);
        } catch (const std::exception& e) {
            qCWarning(libreCelikAPI) << "Can not read PIN tries on reader: " << cardReader
                                     << ". Exception: " << e.what();
            emit pinTriesLeftRead(-1, false);
        }
    }
}

void EIdReader::requestVerification(std::unique_ptr<eidcard::EIdCard>& eidCard, LibreSCRS::VerificationOptions options)
{
    if (!eidCard)
        return;

    // Load trusted CA certificates from Qt resources into the verifier.
    // CardVerifier uses std::filesystem which cannot read Qt resource paths,
    // so we enumerate the resources here and pass each cert as raw bytes.
    QDir certDir(":/certificates");
    int certsLoaded = 0;
    for (const QString& name : certDir.entryList(QDir::Files)) {
        QFile f(certDir.filePath(name));
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray data = f.readAll();
            eidCard->addTrustedCertificate(
                std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data.constData()),
                                     reinterpret_cast<const uint8_t*>(data.constData()) + data.size()));
            certsLoaded++;
        }
    }
    qDebug(libreCelikAPI) << "Loaded" << certsLoaded << "trusted certificates from :/certificates";

    if (options.testFlag(LibreSCRS::VerificationOption::CheckCard)) {
        try {
            emit cardVerificationResultRead(eidCard->verifyCard());
        } catch (const std::exception& e) {
            qCWarning(libreCelikAPI) << "Card verification failed:" << e.what();
            emit cardVerificationResultRead(eidcard::VerificationResult::Unknown);
        }
    }

    if (options.testFlag(LibreSCRS::VerificationOption::CheckSignature)) {
        try {
            emit fixedVerificationResultRead(eidCard->verifyFixedData());
        } catch (const std::exception& e) {
            qCWarning(libreCelikAPI) << "Fixed data verification failed:" << e.what();
            emit fixedVerificationResultRead(eidcard::VerificationResult::Unknown);
        }

        try {
            emit variableVerificationResultRead(eidCard->verifyVariableData());
        } catch (const std::exception& e) {
            qCWarning(libreCelikAPI) << "Variable data verification failed:" << e.what();
            emit variableVerificationResultRead(eidcard::VerificationResult::Unknown);
        }
    }
}

void EIdReader::requestPINTriesLeft()
{
    futurePinData = std::async(std::launch::async, [this]() {
        if (!eidCard) {
            emit pinTriesLeftRead(-1, false);
            return;
        }
        try {
            auto result = eidCard->getPINTriesLeft();
            emit pinTriesLeftRead(result.retriesLeft, result.blocked);
        } catch (const std::exception& e) {
            qCWarning(libreCelikAPI) << "Failed to get PIN tries left:" << e.what();
            emit pinTriesLeftRead(-1, false);
        }
    });
}

void EIdReader::requestChangePIN(const QString& oldPin, const QString& newPin)
{
    futurePinData = std::async(std::launch::async, [this, oldPin, newPin]() {
        if (!eidCard) {
            emit pinChangeFailed(-1, false, qtTrId("lc-eidreader-failed-connect"));
            return;
        }
        try {
            auto result = eidCard->changePIN(oldPin.toStdString(), newPin.toStdString());
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
            qCWarning(libreCelikAPI) << "PIN change failed:" << e.what();
            emit pinChangeFailed(-1, false,
                                 qtTrId("lc-eidreader-pin-change-exception").arg(QString::fromStdString(e.what())));
        }
    });
}

void EIdReader::requestPhoto(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    if (!eidCard)
        return;

    try {
        emit photoDataRead(eidCard->readPortrait());
    } catch (std::runtime_error& re) {
        qCWarning(libreCelikAPI) << "Can not read photo on reader: " << cardReader << ". Exception: " << re.what();
    }
}
