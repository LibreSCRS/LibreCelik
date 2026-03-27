// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QCoreApplication>
#include <QDate>
#include "pivtextdocument.h"
#include <plugin/carddatautils.h>

using plugin::getFieldValue;

PIVTextDocument::PIVTextDocument(const plugin::CardData& cardData, QString cssPath)
{
    auto html = buildHtml(cardData);
    setupDocument(html, cssPath);
}

QString PIVTextDocument::emitRow(const QString& label, const QString& value) const
{
    if (value.isEmpty())
        return {};

    return QString("<tr>"
                   "<td width=\"0\"><img src=\":/images/transparent_1x20.png\" width=\"1\" height=\"8\"></td>"
                   "<td width=\"25%%\"><b>%1:</b></td>"
                   "<td>%2</td>"
                   "</tr>\n")
        .arg(label.toHtmlEscaped(), value.toHtmlEscaped());
}

QString PIVTextDocument::buildHtml(const plugin::CardData& cardData) const
{
    QString html;
    html += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
            "<head><title>" +
            qtTrId("lc-piv-doc-title") +
            "</title>\n"
            "<link rel=\"stylesheet\" type=\"text/css\" href=\":/html/pivcard.css\" title=\"Style\"/>\n"
            "</head>\n<body>\n";

    html += "<h1>" + qtTrId("lc-piv-doc-title") + "</h1>\n";

    // Name as subtitle if available
    auto name = getFieldValue(cardData, "name");
    if (!name.isEmpty())
        html += "<h2>" + getPreparedValue(name).toHtmlEscaped() + "</h2>\n";

    html += buildChuidSection(cardData);
    html += buildCccSection(cardData);
    html += buildPrintedSection(cardData);
    html += buildDiscoverySection(cardData);
    html += buildKeyHistorySection(cardData);

    // Printing date
    html += "<table style=\"margin-top:20px;\"><tr>"
            "<td width=\"0\"><img src=\":/images/transparent_1x20.png\" width=\"1\" height=\"8\"></td>"
            "<td width=\"25%\">" +
            qtTrId("lc-piv-doc-printing-date") +
            ":</td>"
            "<td>" +
            QDate::currentDate().toString("dd.MM.yyyy") +
            "</td>"
            "</tr></table>\n";

    html += "</body>\n</html>\n";
    return html;
}

QString PIVTextDocument::buildChuidSection(const plugin::CardData& cardData) const
{
    QString rows;
    rows += emitRow(qtTrId("lc-piv-field-guid"), getFieldValue(cardData, "guid"));
    rows += emitRow(qtTrId("lc-piv-field-fascn"), getFieldValue(cardData, "fascn"));
    rows += emitRow(qtTrId("lc-piv-field-expiration"), getFieldValue(cardData, "expirationDate"));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-chuid") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString PIVTextDocument::buildCccSection(const plugin::CardData& cardData) const
{
    auto row = emitRow(qtTrId("lc-piv-field-cardid"), getFieldValue(cardData, "cardIdentifier"));
    if (row.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-ccc") + "</h2>\n<table>\n" + row + "</table>\n";
}

QString PIVTextDocument::buildPrintedSection(const plugin::CardData& cardData) const
{
    struct Field
    {
        const char* key;
        QString label;
    };
    std::vector<Field> fields = {
        {"name", qtTrId("lc-piv-field-name")},
        {"employeeAffiliation", qtTrId("lc-piv-field-affiliation")},
        {"org1", qtTrId("lc-piv-field-org1")},
        {"org2", qtTrId("lc-piv-field-org2")},
        {"expiry", qtTrId("lc-piv-field-expiration")},
        {"serialNumber", qtTrId("lc-piv-field-serial")},
        {"issuerId", qtTrId("lc-piv-field-issuer")},
    };

    QString rows;
    for (const auto& f : fields)
        rows += emitRow(f.label, getFieldValue(cardData, f.key));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-printed") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString PIVTextDocument::buildDiscoverySection(const plugin::CardData& cardData) const
{
    auto row = emitRow(qtTrId("lc-piv-field-pinpolicy"), getFieldValue(cardData, "pinPolicy"));
    if (row.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-discovery") + "</h2>\n<table>\n" + row + "</table>\n";
}

QString PIVTextDocument::buildKeyHistorySection(const plugin::CardData& cardData) const
{
    QString rows;
    rows += emitRow(qtTrId("lc-piv-field-oncardcerts"), getFieldValue(cardData, "onCardCerts"));
    rows += emitRow(qtTrId("lc-piv-field-offcardcerts"), getFieldValue(cardData, "offCardCerts"));
    rows += emitRow(qtTrId("lc-piv-field-offcardurl"), getFieldValue(cardData, "offCardURL"));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-keyhistory") + "</h2>\n<table>\n" + rows + "</table>\n";
}
