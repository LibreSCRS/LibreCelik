// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QApplication>
#include <thread>
#include <gtest/gtest.h>
#include "eidreader_test.h"

static const std::string CARD_READER = "HID Global OMNIKEY 5422 Smartcard Reader";

using namespace std::literals;

EIdReaderListener::EIdReaderListener(QObject* parent) : QObject(parent)
{
    eIDReader = std::make_unique<EIdReader>(CARD_READER);

    connect(eIDReader.get(), &EIdReader::cardVersionRead, [](CelikAPI::CardVersion data)
            {
                EXPECT_NE(data, CelikAPI::CardVersion::Unknown);
            }
            );

    connect(eIDReader.get(), &EIdReader::fixedPersonalDataRead, [](CelikAPI::FixedPersonalData data)
            {
                EXPECT_NE(data.givenName.toStdString(), "");
                EXPECT_NE(data.surname.toStdString(), "");
                EXPECT_NE(data.parentGivenName.toStdString(), "");
                EXPECT_NE(data.sex.toStdString(), "");
                EXPECT_NE(data.personalNumber.toStdString(), "");
                EXPECT_NE(data.personalNumber.toStdString(), "");
                EXPECT_NE(data.dateOfBirth.toStdString(), "");
                EXPECT_NE(data.placeOfBirth.toStdString(), "");
            }
            );

    connect(eIDReader.get(), &EIdReader::variablePersonalDataRead, [](CelikAPI::VariablePersonalData data)
            {
                EXPECT_NE(data.address.toStdString(), "");
                EXPECT_NE(data.addressDate.toStdString(), "");
            }
            );

    connect(eIDReader.get(), &EIdReader::documentDataRead, [](CelikAPI::DocumentData data)
            {
                EXPECT_NE(data.docRegNo.toStdString(), "");
                EXPECT_NE(data.expiryDate.toStdString(), "");
                EXPECT_NE(data.issuingDate.toStdString(), "");
                EXPECT_NE(data.issuingAuthority.toStdString(), "");
            }
            );

    connect(eIDReader.get(), &EIdReader::photoDataRead, [](CelikAPI::PhotoData data)
            {
                EXPECT_NE(data.size(), 0);
            }
            );

    connect(eIDReader.get(), &EIdReader::cardSignatureVerificationResultRead, [](CelikAPI::VerificationResult data)
            {
                EXPECT_EQ(data, CelikAPI::VerificationResult::Good);
            }
            );

    connect(eIDReader.get(), &EIdReader::fixedSignatureVerificationResultRead, [](CelikAPI::VerificationResult data)
            {
                EXPECT_EQ(data, CelikAPI::VerificationResult::Good);
            }
            );

    connect(eIDReader.get(), &EIdReader::variableSignatureVerificationResultRead, [](CelikAPI::VerificationResult data)
            {
                EXPECT_EQ(data, CelikAPI::VerificationResult::Good);
            }
            );

    // eIDReader->requestVerification(CelikAPI::VerificationOption::CheckCard);
    // eIDReader->requestVerification(CelikAPI::VerificationOption::CheckSignature);
    // eIDReader->requestVerification(CelikAPI::VerificationOption::NoCheck);
    eIDReader->requestData();
}

EIdReaderListener::~EIdReaderListener()
{
    eIDReader.reset();
}

class EIdReaderTest : public ::testing::Test
{

    int argc = 0;

protected:
    virtual void SetUp()
    {
        qRegisterMetaType<CelikAPI::FixedPersonalData>();
        listener = new EIdReaderListener();
    }

    virtual void TearDown()
    {
        delete listener;
        delete app;
    }

    QCoreApplication* app = new QCoreApplication (argc, nullptr);
    EIdReaderListener* listener;
};


TEST_F(EIdReaderTest, ListenerTest)
{
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(10s);
}
