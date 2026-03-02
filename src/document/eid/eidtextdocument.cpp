// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QCoreApplication>
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
    data.replace("${title}", qtTrId("lc-eid-doc-title"));
    data.replace("${foreigner_id}", qtTrId("lc-eid-doc-foreigner-id"));
    data.replace("${printing_date}", qtTrId("lc-eid-doc-printing-date"));
    data.replace("${printing_date_value}", QDate::currentDate().toString("dd.MM.yyyy"));

    data.replace("${foreigner_data}", qtTrId("lc-eid-foreigner-data"));
    data.replace("${citizen_data}", qtTrId("lc-eid-citizen-data"));
    data.replace("${last_name}", qtTrId("lc-eid-doc-surname"));
    data.replace("${first_name}", qtTrId("lc-eid-doc-name"));
    data.replace("${parent_name}", qtTrId("lc-eid-doc-parent-name"));
    data.replace("${nationality}", qtTrId("lc-eid-doc-nationality"));
    data.replace("${date_of_birth}", qtTrId("lc-eid-doc-date-birth"));
    data.replace("${place_of_birth}", qtTrId("lc-eid-doc-place-birth"));
    data.replace("${status_of_foreigner}", qtTrId("lc-eid-doc-foreigner-status"));

    if(isForeigner)
        data.replace("${adress}", qtTrId("lc-eid-label-address-foreigner"));
    else
        data.replace("${adress}", qtTrId("lc-eid-label-address"));

    data.replace("${date_of_address_change}", qtTrId("lc-eid-doc-address-change-date"));
    data.replace("${jmbg}", qtTrId("lc-eid-label-jmbg"));
    data.replace("${gender}", qtTrId("lc-eid-doc-gender"));

    data.replace("${document_data}", qtTrId("lc-eid-doc-document-data"));
    data.replace("${document_issuer}", qtTrId("lc-eid-doc-issuer"));
    data.replace("${document_number}", qtTrId("lc-eid-doc-number"));
    data.replace("${issuance_date}", qtTrId("lc-eid-doc-issuance-date"));
    data.replace("${validity_date}", qtTrId("lc-eid-doc-valid-to"));
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
