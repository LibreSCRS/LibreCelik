// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QDate>
#include "eidtextdocument.h"

EIdTextDocument::EIdTextDocument(const eidcard::FixedPersonalData& fixedPersonalData,
                                 const eidcard::VariablePersonalData& variablePersonalData,
                                 const eidcard::DocumentData& documentData,
                                 const QString& address,
                                 const QString& placeOfBirth,
                                 const QString &photo,
                                 QString documentPath,
                                 QString cssPath)
{
    auto data = loadFile(documentPath);

    isForeigner = documentPath.contains("/idcardIF20", Qt::CaseInsensitive);

    translateDocumentData(data);
    prepareDocumentData(data, fixedPersonalData, variablePersonalData, documentData, address, placeOfBirth, photo);

    setupDocument(data, cssPath);
}

void EIdTextDocument::translateDocumentData(QString& data) const
{
    data.replace("${title}", QCoreApplication::translate("CelikTextDocument", "ELECTRONIC ID CARD READER: DATA PRINTING"));
    data.replace("${foreigner_id}",QCoreApplication::translate("CelikTextDocument", "Foreigner id"));
    data.replace("${printing_date}",QCoreApplication::translate("CelikTextDocument", "Printing date"));
    data.replace("${printing_date_value}",QDate::currentDate().toString("dd.MM.yyyy"));

    data.replace("${foreigner_data}", QCoreApplication::translate("EId", "Foreigner Data"));
    data.replace("${citizen_data}", QCoreApplication::translate("EId", "Citizen Data"));
    data.replace("${last_name}", QCoreApplication::translate("EId", "Surname"));
    data.replace("${first_name}", QCoreApplication::translate("EId", "Name"));
    data.replace("${parent_name}", QCoreApplication::translate("EId", "Parent name"));
    data.replace("${nationality}", QCoreApplication::translate("EId", "Nationality"));
    data.replace("${date_of_birth}", QCoreApplication::translate("EId", "Date of birth"));
    data.replace("${place_of_birth}", QCoreApplication::translate("EId", "Place of birth"));
    data.replace("${status_of_foreigner}", QCoreApplication::translate("EId", "Status of foreigner"));

    if(isForeigner)
        data.replace("${adress}", QCoreApplication::translate("EId", "Address", "foreigner"));
    else
        data.replace("${adress}", QCoreApplication::translate("EId", "Address"));

    data.replace("${date_of_address_change}", QCoreApplication::translate("EId", "Date of address change"));
    data.replace("${jmbg}", QCoreApplication::translate("EId", "JMBG"));
    data.replace("${gender}", QCoreApplication::translate("EId", "Gender"));

    data.replace("${document_data}", QCoreApplication::translate("EId", "Document data"));
    data.replace("${document_issuer}", QCoreApplication::translate("EId", "Document issuer"));
    data.replace("${document_number}", QCoreApplication::translate("EId", "Document number"));
    data.replace("${issuance_date}", QCoreApplication::translate("EId", "Date of issuance"));
    data.replace("${validity_date}", QCoreApplication::translate("EId", "Valid to"));
}

void EIdTextDocument::prepareDocumentData(QString& data, const eidcard::FixedPersonalData &fixedPersonalData, const eidcard::VariablePersonalData &variablePersonalData, const eidcard::DocumentData &documentData, const QString& address, const QString& placeOfBirth, const QString &photo) const
{
    data.replace("${last_name_value}", getPreparedValue(QString::fromStdString(fixedPersonalData.surname)));
    data.replace("${first_name_value}", getPreparedValue(QString::fromStdString(fixedPersonalData.givenName)));
    data.replace("${parent_name_value}", getPreparedValue(QString::fromStdString(fixedPersonalData.parentGivenName)));
    data.replace("${nationality_value}", getPreparedValue(QString::fromStdString(fixedPersonalData.nationalityFull)));
    data.replace("${date_of_birth_value}", getPreparedValue(QString::fromStdString(fixedPersonalData.dateOfBirth)));
    data.replace("${place_of_birth_value}", getPreparedValue(placeOfBirth));
    data.replace("${status_of_foreigner_value}", getPreparedValue(QString::fromStdString(fixedPersonalData.statusOfForeigner)));
    data.replace("${adress_value}", getPreparedValue(address));
    data.replace("${date_of_address_change_value}", getPreparedValue(QString::fromStdString(variablePersonalData.addressDate)));
    data.replace("${jmbg_value}", getPreparedValue(QString::fromStdString(fixedPersonalData.personalNumber)));
    data.replace("${gender_value}", getPreparedValue(QString::fromStdString(fixedPersonalData.sex)));

    data.replace("${document_issuer_value}", getPreparedValue(QString::fromStdString(documentData.issuingAuthority)));
    data.replace("${document_number_value}", getPreparedValue(QString::fromStdString(documentData.docRegNo)));
    data.replace("${issuance_date_value}", getPreparedValue(QString::fromStdString(documentData.issuingDate)));
    data.replace("${validity_date_value}", getPreparedValue(QString::fromStdString(documentData.expiryDate)));

    QString str = "data:image/png;base64, " + photo;
    data.replace(":/images/user.png", str);
}
