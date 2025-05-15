// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QTextDocument>
#include <QFile>
#include <QDate>
#include <QPrinter>
#include <QTranslator>
#include <QFontDatabase>
#include "utils/libreceliklog.h"
#include "eidtextdocument.h"
#include "config.h"

EIdTextDocument::EIdTextDocument(const CelikAPI::FixedPersonalData& fixedPersonalData,
                                 const CelikAPI::VariablePersonalData& variablePersonalData,
                                 const CelikAPI::DocumentData& documentData,
                                 const QString &photo,
                                 QString documentPath,
                                 QString cssPath)
{
    QString data;
    QString fileName(documentPath);
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) {
        qCDebug(libreCelikPrinting) << "File not opened in readonly";
    }
    else
    {
        data = file.readAll();
    }
    file.close();

    isForeigner = documentPath.contains("/idcardIF20", Qt::CaseInsensitive);

    translateDocumentData(data);
    prepareDocumentData(data, fixedPersonalData, variablePersonalData, documentData, photo);

    document.setPageSize(QPageSize::size(QPageSize::A4, QPageSize::Unit::Point));
    document.setHtml(data);

    file.setFileName(cssPath);
    if(!file.open(QIODevice::ReadOnly)) {
        qCDebug(libreCelikPrinting) << "File not opened in readonly";
    }
    else
    {
        data = file.readAll();
    }
    file.close();

    document.setDefaultStyleSheet(data);
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    
    // HACK: Set bigger fonts for macos
    font.setPointSize(isMacOS() ? MACOS_FONTSIZE : LINUX_FONTSIZE);

    document.setDefaultFont(font);

    qCDebug(libreCelikPrinting) << "Document page size: " << document.pageSize();
    qCDebug(libreCelikPrinting) << "Page count: " << document.pageCount();
}

void EIdTextDocument::print(QPrinter *printer) const
{
    qCDebug(libreCelikPrinting) << "Printer page rect: " << printer->pageRect(QPrinter::Unit::Point).size();
    QMarginsF margins( 21, 29.7, 21, 29.7 );
    printer->setPageMargins( margins, QPageLayout::Millimeter );
    document.print( printer );
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

void EIdTextDocument::prepareDocumentData(QString& data, const CelikAPI::FixedPersonalData &fixedPersonalData, const CelikAPI::VariablePersonalData &variablePersonalData, const CelikAPI::DocumentData &documentData, const QString &photo) const
{
    data.replace("${last_name_value}", getPreparedValue(fixedPersonalData.surname));
    data.replace("${first_name_value}", getPreparedValue(fixedPersonalData.givenName));
    data.replace("${parent_name_value}", getPreparedValue(fixedPersonalData.parentGivenName));
    data.replace("${nationality_value}", getPreparedValue(fixedPersonalData.nationalityFull));
    data.replace("${date_of_birth_value}", getPreparedValue(fixedPersonalData.dateOfBirth));
    data.replace("${place_of_birth_value}", getPreparedValue(fixedPersonalData.placeOfBirth));
    data.replace("${status_of_foreigner_value}", getPreparedValue(fixedPersonalData.statusOfForeigner));
    data.replace("${adress_value}", getPreparedValue(variablePersonalData.address));
    data.replace("${date_of_address_change_value}", getPreparedValue(variablePersonalData.addressDate));
    data.replace("${jmbg_value}", getPreparedValue(fixedPersonalData.personalNumber));
    data.replace("${gender_value}", getPreparedValue(fixedPersonalData.sex));

    data.replace("${document_issuer_value}", getPreparedValue(documentData.issuingAuthority));
    data.replace("${document_number_value}", getPreparedValue(documentData.docRegNo));
    data.replace("${issuance_date_value}", getPreparedValue(documentData.issuingDate));
    data.replace("${validity_date_value}", getPreparedValue(documentData.expiryDate));

    QString str = "data:image/png;base64, " + photo;
    data.replace(":/images/user.png", str);
}

QString EIdTextDocument::getPreparedValue(const QString& data) const
{
    // Data should not be translated. It's comming from the card.
    return (data.isEmpty() || data=="01.01.0001") ? QCoreApplication::translate("CelikTextDocument", "Unavailable") : data;
}

bool EIdTextDocument::isMacOS() const
{
    return LIBRECELIK_SYSTEM_NAME == MACOS;
}

bool EIdTextDocument::isLinux() const
{
    return LIBRECELIK_SYSTEM_NAME == LINUX;
}
