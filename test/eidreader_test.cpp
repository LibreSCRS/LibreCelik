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

    connect(eIDReader.get(), &EIdReader::cardTypeRead,
            [](eidcard::CardType data) { EXPECT_NE(data, eidcard::CardType::Unknown); });

    connect(eIDReader.get(), &EIdReader::fixedPersonalDataRead, [](eidcard::FixedPersonalData data) {
        EXPECT_NE(data.givenName, "");
        EXPECT_NE(data.surname, "");
        EXPECT_NE(data.parentGivenName, "");
        EXPECT_NE(data.sex, "");
        EXPECT_NE(data.personalNumber, "");
        EXPECT_NE(data.personalNumber, "");
        EXPECT_NE(data.dateOfBirth, "");
        EXPECT_NE(data.placeOfBirth, "");
    });

    connect(eIDReader.get(), &EIdReader::variablePersonalDataRead, [](eidcard::VariablePersonalData data) {
        EXPECT_NE(data.place, "");
        EXPECT_NE(data.addressDate, "");
    });

    connect(eIDReader.get(), &EIdReader::documentDataRead, [](eidcard::DocumentData data) {
        EXPECT_NE(data.docRegNo, "");
        EXPECT_NE(data.expiryDate, "");
        EXPECT_NE(data.issuingDate, "");
        EXPECT_NE(data.issuingAuthority, "");
    });

    connect(eIDReader.get(), &EIdReader::photoDataRead, [](eidcard::PhotoData data) { EXPECT_NE(data.size(), 0); });

    connect(eIDReader.get(), &EIdReader::cardVerificationResultRead,
            [](eidcard::VerificationResult data) { EXPECT_EQ(data, eidcard::VerificationResult::Valid); });

    connect(eIDReader.get(), &EIdReader::fixedVerificationResultRead,
            [](eidcard::VerificationResult data) { EXPECT_EQ(data, eidcard::VerificationResult::Valid); });

    connect(eIDReader.get(), &EIdReader::variableVerificationResultRead,
            [](eidcard::VerificationResult data) { EXPECT_EQ(data, eidcard::VerificationResult::Valid); });

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
        qRegisterMetaType<eidcard::FixedPersonalData>();
        listener = new EIdReaderListener();
    }

    virtual void TearDown()
    {
        delete listener;
        delete app;
    }

    QCoreApplication* app = new QCoreApplication(argc, nullptr);
    EIdReaderListener* listener;
};

TEST_F(EIdReaderTest, ListenerTest)
{
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(10s);
}
