// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QString>
#include <QStringList>
#include <gtest/gtest.h>
#include <eidcard/eidcard.h>

static const std::string CARD_READER = "HID Global OMNIKEY 5422 Smartcard Reader";

class EIdCardTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        try {
            eidCard = std::make_unique<eidcard::EIdCard>(CARD_READER);
        } catch (std::runtime_error& ex) {
            FAIL() << ex.what();
        }
    }

    virtual void TearDown()
    {
        eidCard.reset();
    }

    std::unique_ptr<eidcard::EIdCard> eidCard;
};

TEST_F(EIdCardTest, ReadCardVersion)
{
    auto cardType = eidCard->getCardType();
    EXPECT_NE(static_cast<int>(cardType), 0);
}

TEST_F(EIdCardTest, ReadDocumentData)
{
    try {
        auto data = eidCard->readDocumentData();
        EXPECT_NE(data.docRegNo, "");
        EXPECT_NE(data.issuingAuthority, "");
        EXPECT_NE(data.issuingDate, "");
        EXPECT_NE(data.expiryDate, "");
    } catch (std::runtime_error& err) {
        FAIL() << err.what();
    }
}

TEST_F(EIdCardTest, ReadFixedPersonalData)
{
    try {
        auto fpd = eidCard->readFixedPersonalData();
        EXPECT_NE(fpd.givenName, "");
        EXPECT_NE(fpd.surname, "");
        EXPECT_NE(fpd.sex, "");
        EXPECT_NE(fpd.personalNumber, "");
        EXPECT_NE(fpd.dateOfBirth, "");
    } catch (std::runtime_error& err) {
        FAIL() << err.what();
    }
}

TEST_F(EIdCardTest, ReadVariablePersonalData)
{
    try {
        auto vpd = eidCard->readVariablePersonalData();
        EXPECT_NE(vpd.place, "");
        EXPECT_NE(vpd.addressDate, "");
    } catch (std::runtime_error& err) {
        FAIL() << err.what();
    }
}

TEST_F(EIdCardTest, ReadPhotoData)
{
    try {
        auto photo = eidCard->readPortrait();
        EXPECT_NE(photo.size(), 0);
    } catch (std::runtime_error& err) {
        FAIL() << err.what();
    }
}
