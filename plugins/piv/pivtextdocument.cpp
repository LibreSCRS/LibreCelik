// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <QCoreApplication>
#include <QDate>
#include "pivtextdocument.h"
#include <plugin/fieldvalue.h>

using librecelik::plugin::fieldValue;
using LibreSCRS::AgentClient::FieldGroup;

PIVTextDocument::PIVTextDocument(const QList<FieldGroup>& groups, QString cssPath)
{
    auto html = buildHtml(groups);
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

QString PIVTextDocument::buildHtml(const QList<FieldGroup>& groups) const
{
    QString html;
    html +=
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
        "<head><title>" +
        qtTrId("lc-piv-doc-title") + // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
        "</title>\n"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\":/html/pivcard.css\" title=\"Style\"/>\n"
        "</head>\n<body>\n";

    html += "<h1>" + qtTrId("lc-piv-doc-title") + "</h1>\n";

    // Name as subtitle if available
    auto name = fieldValue(groups, u"name");
    if (!name.isEmpty())
        html += "<h2>" + getPreparedValue(name).toHtmlEscaped() + "</h2>\n";

    html += buildChuidSection(groups);
    html += buildCccSection(groups);
    html += buildPrintedSection(groups);
    html += buildDiscoverySection(groups);
    html += buildKeyHistorySection(groups);

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

QString PIVTextDocument::buildChuidSection(const QList<FieldGroup>& groups) const
{
    QString rows;
    rows += emitRow(qtTrId("lc-piv-field-guid"), fieldValue(groups, u"guid"));
    rows += emitRow(qtTrId("lc-piv-field-fascn"), fieldValue(groups, u"fascn"));
    rows += emitRow(qtTrId("lc-piv-field-expiration"), fieldValue(groups, u"expirationDate"));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-chuid") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString PIVTextDocument::buildCccSection(const QList<FieldGroup>& groups) const
{
    auto row = emitRow(qtTrId("lc-piv-field-cardid"), fieldValue(groups, u"cardIdentifier"));
    if (row.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-ccc") + "</h2>\n<table>\n" + row + "</table>\n";
}

QString PIVTextDocument::buildPrintedSection(const QList<FieldGroup>& groups) const
{
    struct Field
    {
        QStringView key;
        QString label;
    };
    std::vector<Field> fields = {
        {u"name", qtTrId("lc-piv-field-name")},         {u"employeeAffiliation", qtTrId("lc-piv-field-affiliation")},
        {u"org1", qtTrId("lc-piv-field-org1")},         {u"org2", qtTrId("lc-piv-field-org2")},
        {u"expiry", qtTrId("lc-piv-field-expiration")}, {u"serialNumber", qtTrId("lc-piv-field-serial")},
        {u"issuerId", qtTrId("lc-piv-field-issuer")},
    };

    QString rows;
    for (const auto& f : fields)
        rows += emitRow(f.label, fieldValue(groups, f.key));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-printed") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString PIVTextDocument::buildDiscoverySection(const QList<FieldGroup>& groups) const
{
    auto row = emitRow(qtTrId("lc-piv-field-pinpolicy"), fieldValue(groups, u"pinPolicy"));
    if (row.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-discovery") + "</h2>\n<table>\n" + row + "</table>\n";
}

QString PIVTextDocument::buildKeyHistorySection(const QList<FieldGroup>& groups) const
{
    QString rows;
    rows += emitRow(qtTrId("lc-piv-field-oncardcerts"), fieldValue(groups, u"onCardCerts"));
    rows += emitRow(qtTrId("lc-piv-field-offcardcerts"), fieldValue(groups, u"offCardCerts"));
    rows += emitRow(qtTrId("lc-piv-field-offcardurl"), fieldValue(groups, u"offCardURL"));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-piv-section-keyhistory") + "</h2>\n<table>\n" + rows + "</table>\n";
}
