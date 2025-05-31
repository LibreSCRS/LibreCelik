// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <celikapi.h>
#include <QString>
#include <QStringList>
#include <gtest/gtest.h>
#include "celikapi/celikapisession.h"
#include "celikapi/celikapisessionfactory.h"

static const std::string CARD_READER = "HID Global OMNIKEY 5422 Smartcard Reader";

class CelikAPITest : public ::testing::Test
{
protected:

    virtual void SetUp()
    {
        try
        {
            celikSession = CelikAPISessionFactory::instance().createCelikAPISession(CARD_READER);
        }
        catch(std::runtime_error& ex)
        {
            FAIL();
        }
    }

    virtual void TearDown()
    {
        celikSession.reset();
    }

    std::unique_ptr<CelikAPISession> celikSession;
};

TEST_F(CelikAPITest, ReadCardVersion)
{
    try
    {
        auto cardVersion = celikSession->getCardVersion();

        EXPECT_NE(cardVersion, 0);
    }
    catch(std::runtime_error& err)
    {
        FAIL();
    }
}

TEST_F(CelikAPITest, ReadDocumentData)
{
    EID_DOCUMENT_DATA data;
    try
    {
        celikSession->readDocumentData(&data);
        QString docRegNo = QString::fromUtf8(data.docRegNo, data.docRegNoSize);
        EXPECT_NE(docRegNo.toStdString(), "");

        QString issuingAuthority = QString::fromUtf8(data.issuingAuthority, data.issuingAuthoritySize);
        EXPECT_NE(issuingAuthority.toStdString(), "");

        QString issuingDate = QString::fromUtf8(data.issuingDate, data.issuingDateSize);
        EXPECT_NE(issuingDate.toStdString(), "");

        QString expiryDate = QString::fromUtf8(data.expiryDate, data.expiryDateSize);
        EXPECT_NE(expiryDate.toStdString(), "");
    }
    catch(std::runtime_error& err)
    {
        FAIL();
    }
}

TEST_F(CelikAPITest, ReadFixedPersonalData)
{
    EID_FIXED_PERSONAL_DATA eidFixedPersonalData;
    try
    {
        celikSession->readFixedPersonalData(&eidFixedPersonalData);

        QString givenName = QString::fromUtf8(eidFixedPersonalData.givenName, eidFixedPersonalData.givenNameSize);
        EXPECT_NE(givenName.toStdString(), "");
        QString surname = QString::fromUtf8(eidFixedPersonalData.surname, eidFixedPersonalData.surnameSize);
        EXPECT_NE(surname.toStdString(), "");
        QString parentGivenName = QString::fromUtf8(eidFixedPersonalData.parentGivenName, eidFixedPersonalData.parentGivenNameSize);
        EXPECT_NE(parentGivenName.toStdString(), "");
        //QString nationalityFull = QString::fromUtf8(eidFixedPersonalData.nationalityFull, eidFixedPersonalData.nationalityFullSize);
        //EXPECT_NE(nationalityFull.toStdString(), "");
        QString sex = QString::fromUtf8(eidFixedPersonalData.sex, eidFixedPersonalData.sexSize);
        EXPECT_NE(sex.toStdString(), "");
        QString personalNumber = QString::fromUtf8(eidFixedPersonalData.personalNumber, eidFixedPersonalData.personalNumberSize);
        EXPECT_NE(personalNumber.toStdString(), "");
        QString dateOfBirth = QString::fromUtf8(eidFixedPersonalData.dateOfBirth, eidFixedPersonalData.dateOfBirthSize);
        EXPECT_NE(dateOfBirth.toStdString(), "");
        //QString statusOfForeigner = QString::fromUtf8(eidFixedPersonalData.statusOfForeigner, eidFixedPersonalData.statusOfForeignerSize);
        //EXPECT_NE(statusOfForeigner.toStdString(), "");

        QStringList list;
        list << QString::fromUtf8(eidFixedPersonalData.placeOfBirth, eidFixedPersonalData.placeOfBirthSize)
             << QString::fromUtf8(eidFixedPersonalData.communityOfBirth, eidFixedPersonalData.communityOfBirthSize)
             << QString::fromUtf8(eidFixedPersonalData.stateOfBirth, eidFixedPersonalData.stateOfBirthSize);
        auto birth = list.join(", ");

        EXPECT_NE(birth.toStdString(), "");
    }
    catch(std::runtime_error& err)
    {
        FAIL();
    }
}

TEST_F(CelikAPITest, ReadVariablePersonalData)
{
    EID_VARIABLE_PERSONAL_DATA eidVariablePersonalData;
    try
    {
        celikSession->readVariablePersonalData(&eidVariablePersonalData);

        QString addressDate = QString::fromUtf8(eidVariablePersonalData.addressDate, eidVariablePersonalData.addressDateSize);
        EXPECT_NE(addressDate.toStdString(), "");

        QStringList list;
        list << QString::fromUtf8(eidVariablePersonalData.place, eidVariablePersonalData.placeSize)
             << QString::fromUtf8(eidVariablePersonalData.community, eidVariablePersonalData.communitySize)
             << QString::fromUtf8(eidVariablePersonalData.street, eidVariablePersonalData.streetSize)
             << QString::fromUtf8(eidVariablePersonalData.houseNumber, eidVariablePersonalData.houseNumberSize);
        QString apartmentNumber = QString::fromUtf8(eidVariablePersonalData.apartmentNumber, eidVariablePersonalData.apartmentNumberSize);
        QString floor = QString::fromUtf8(eidVariablePersonalData.floor, eidVariablePersonalData.floorSize);
        auto address = floor.isEmpty() ? list.join(", ") : list.join(", ") + "/" + floor;
        auto addressFull = apartmentNumber.isEmpty() ? address : address + "/" + apartmentNumber;

        EXPECT_NE(addressFull.toStdString(), "");
    }
    catch(std::runtime_error& err)
    {
        FAIL();
    }
}

TEST_F(CelikAPITest, ReadPhotoData)
{
    EID_PORTRAIT eidPortrait;
    try
    {
        celikSession->readPortrait(&eidPortrait);

        std::vector<uint8_t> photo;
        photo.resize(eidPortrait.portraitSize);
        std::copy_n(eidPortrait.portrait, eidPortrait.portraitSize, std::begin(photo));
        EXPECT_NE(photo.size(), 0);
    }
    catch(std::runtime_error& err)
    {
        FAIL();
    }
}

TEST_F(CelikAPITest, VeifySigCard)
{
    try
    {
        celikSession->verifySignature(EID_SIG_CARD);
        SUCCEED();
    }
    catch(std::runtime_error& err)
    {
        FAIL();
    }
}

TEST_F(CelikAPITest, VeifySigFixed)
{
    try
    {
        celikSession->verifySignature(EID_SIG_FIXED);
        SUCCEED();
    }
    catch(std::runtime_error& err)
    {
        FAIL();
    }
}

TEST_F(CelikAPITest, VeifySigVariable)
{
    try
    {
        celikSession->verifySignature(EID_SIG_VARIABLE);
        SUCCEED();
    }
    catch(std::runtime_error& err)
    {
        FAIL();
    }
}
