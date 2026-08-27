// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Synthetic card-payload factories for D9 plugin retranslate tests.
///
/// Each `make<Plugin>Mock()` returns a minimal-but-complete payload
/// sufficient to populate every section the corresponding plugin widget
/// renders. No PCSC, no real card hardware. Used by
/// `i18n_plugin_retranslate_test.cpp` as parameter sources.
///
/// Every widget family speaks the agent's field groups now, so the payloads
/// below are field groups: stringified values plus the `labelFallback` and
/// `type` tokens the shared flatten rule reads.

#pragma once

#include <LibreSCRS/AgentClient/Types.h>

#include <QList>
#include <QString>

namespace librecelik::test::i18n::mock {

namespace detail {

inline LibreSCRS::AgentClient::Field textField(const QString& key, const QString& label, const QString& value)
{
    LibreSCRS::AgentClient::Field f;
    f.key = key;
    f.value = value;
    f.extra.insert(QStringLiteral("labelFallback"), label);
    f.extra.insert(QStringLiteral("type"), QStringLiteral("text"));
    return f;
}

inline LibreSCRS::AgentClient::Field dateField(const QString& key, const QString& label, const QString& value)
{
    LibreSCRS::AgentClient::Field f;
    f.key = key;
    f.value = value;
    f.extra.insert(QStringLiteral("labelFallback"), label);
    f.extra.insert(QStringLiteral("type"), QStringLiteral("date"));
    return f;
}

inline LibreSCRS::AgentClient::FieldGroup group(const QString& key, QList<LibreSCRS::AgentClient::Field> fields)
{
    LibreSCRS::AgentClient::FieldGroup g;
    g.key = key;
    g.fields = std::move(fields);
    return g;
}

} // namespace detail

// Field keys below are aligned with the production plugin widget
// translation maps (eidwidget.cpp, emrtdwidget.cpp, etc.). Mismatched
// keys fall through to a literal-key label rendering and would make the
// retranslate test's stable-label heuristic flag the field as a stale
// label after switching language; keep these aligned.

inline QList<LibreSCRS::AgentClient::FieldGroup> makeEidMock()
{
    using namespace detail;
    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    // EidWidget reads "meta" first to build the outer section.
    groups.append(group(QStringLiteral("meta"), {
                                                    textField(QStringLiteral("card_type"), QStringLiteral("Card type"),
                                                              QStringLiteral("Citizen2014")),
                                                }));
    // Personal group keys per eidwidget.cpp::buildPersonalSection().
    groups.append(group(
        QStringLiteral("personal"),
        {
            textField(QStringLiteral("given_name"), QStringLiteral("Given name"), QStringLiteral("Marko")),
            textField(QStringLiteral("surname"), QStringLiteral("Surname"), QStringLiteral("Petrović")),
            dateField(QStringLiteral("date_of_birth"), QStringLiteral("Date of birth"), QStringLiteral("1990-01-01")),
            textField(QStringLiteral("personal_number"), QStringLiteral("JMBG"), QStringLiteral("1234567890123")),
            textField(QStringLiteral("sex"), QStringLiteral("Sex"), QStringLiteral("M")),
            textField(QStringLiteral("parent_given_name"), QStringLiteral("Parent name"), QStringLiteral("Petar")),
        }));
    // Address keys per eidwidget.cpp::buildAddressSection().
    groups.append(
        group(QStringLiteral("address"),
              {
                  textField(QStringLiteral("place"), QStringLiteral("Place"), QStringLiteral("Beograd")),
                  textField(QStringLiteral("street"), QStringLiteral("Street"), QStringLiteral("Knez Mihailova")),
                  textField(QStringLiteral("house_number"), QStringLiteral("Number"), QStringLiteral("1")),
              }));
    // Document keys per eidwidget.cpp::buildDocumentSection().
    groups.append(group(
        QStringLiteral("document"),
        {
            textField(QStringLiteral("document_type"), QStringLiteral("Document type"), QStringLiteral("ID")),
            textField(QStringLiteral("document_serial_number"), QStringLiteral("Serial"), QStringLiteral("123456789")),
            textField(QStringLiteral("issuing_authority"), QStringLiteral("Issuer"), QStringLiteral("MUP")),
            dateField(QStringLiteral("expiry_date"), QStringLiteral("Expiry"), QStringLiteral("2030-01-01")),
        }));
    return groups;
}

inline QList<LibreSCRS::AgentClient::FieldGroup> makeHealthMock()
{
    using namespace detail;
    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    // Personal keys per healthwidget.cpp::addPersonalGroup().
    groups.append(
        group(QStringLiteral("personal"),
              {
                  textField(QStringLiteral("given_name"), QStringLiteral("Given name"), QStringLiteral("Ana")),
                  textField(QStringLiteral("family_name"), QStringLiteral("Family name"), QStringLiteral("Jovanović")),
                  textField(QStringLiteral("personal_number"), QStringLiteral("JMBG"), QStringLiteral("1234567890123")),
                  textField(QStringLiteral("insurant_number"), QStringLiteral("LBO"), QStringLiteral("987654321")),
              }));
    // Insurance keys per healthwidget.cpp::insuranceTranslationMap().
    groups.append(group(
        QStringLiteral("insurance"),
        {
            textField(QStringLiteral("insurer_name"), QStringLiteral("Insurer"), QStringLiteral("RFZO")),
            textField(QStringLiteral("insurer_id"), QStringLiteral("Insurer ID"), QStringLiteral("01")),
            dateField(QStringLiteral("date_of_expiry"), QStringLiteral("Expiry"), QStringLiteral("2026-12-31")),
            // Exercises HealthWidget::transformPermanentlyValid (true -> Yes/Да)
            // across a language switch: regression guard that the render-only
            // transform + retranslate rebuild never mutate the source-of-truth.
            textField(QStringLiteral("permanently_valid"), QStringLiteral("Permanently valid"), QStringLiteral("true")),
        }));
    // Address keys per healthwidget.cpp::addressTranslationMap().
    groups.append(
        group(QStringLiteral("address"),
              {
                  textField(QStringLiteral("street"), QStringLiteral("Street"), QStringLiteral("Knez Mihailova")),
                  textField(QStringLiteral("place"), QStringLiteral("Place"), QStringLiteral("Beograd")),
              }));
    return groups;
}

inline QList<LibreSCRS::AgentClient::FieldGroup> makeEmrtdMock()
{
    using namespace detail;
    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    // Personal keys per emrtdwidget.cpp::personalTranslationMap().
    groups.append(group(
        QStringLiteral("personal"),
        {
            textField(QStringLiteral("given_names"), QStringLiteral("Given names"), QStringLiteral("John")),
            textField(QStringLiteral("surname"), QStringLiteral("Surname"), QStringLiteral("Doe")),
            textField(QStringLiteral("nationality"), QStringLiteral("Nationality"), QStringLiteral("USA")),
            dateField(QStringLiteral("date_of_birth"), QStringLiteral("Date of birth"), QStringLiteral("1990-01-01")),
        }));
    // Document keys per emrtdwidget.cpp::documentTranslationMap().
    groups.append(group(
        QStringLiteral("document"),
        {
            textField(QStringLiteral("document_number"), QStringLiteral("Document number"), QStringLiteral("X1234567")),
            textField(QStringLiteral("document_code"), QStringLiteral("Code"), QStringLiteral("P")),
            textField(QStringLiteral("issuing_state"), QStringLiteral("Issuing state"), QStringLiteral("USA")),
            dateField(QStringLiteral("date_of_expiry"), QStringLiteral("Expiry"), QStringLiteral("2030-01-01")),
        }));
    // Presence keys per emrtdwidget.cpp::presenceTranslationMap(); the
    // auth_method value is one of the plugin's closed set, so the localized
    // value dictionary runs under this fixture too.
    groups.append(group(
        QStringLiteral("presence"),
        {
            textField(QStringLiteral("data_groups"), QStringLiteral("Data Groups"), QStringLiteral("DG1, DG2, DG14")),
            textField(QStringLiteral("auth_method"), QStringLiteral("Authentication Method"),
                      QStringLiteral("Chip Authentication")),
        }));
    return groups;
}

inline QList<LibreSCRS::AgentClient::FieldGroup> makeEuVrcMock()
{
    using namespace detail;
    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    // Registration keys per euvrcwidget.cpp::buildRegistrationSection().
    groups.append(group(
        QStringLiteral("registration"),
        {
            textField(QStringLiteral("registration_number"), QStringLiteral("Reg number"), QStringLiteral("BG-123-AA")),
            dateField(QStringLiteral("expiry_date"), QStringLiteral("Expiry"), QStringLiteral("2030-01-01")),
            textField(QStringLiteral("member_state"), QStringLiteral("Member state"), QStringLiteral("RS")),
            textField(QStringLiteral("issuing_authority"), QStringLiteral("Issuing authority"), QStringLiteral("MUP")),
        }));
    // Vehicle keys per euvrcwidget.cpp::buildVehicleSection().
    groups.append(group(
        QStringLiteral("vehicle"),
        {
            textField(QStringLiteral("vehicle_make"), QStringLiteral("Make"), QStringLiteral("VW")),
            textField(QStringLiteral("vehicle_id_number"), QStringLiteral("VIN"), QStringLiteral("WAUZZZ8K3AA000000")),
            textField(QStringLiteral("vehicle_category"), QStringLiteral("Category"), QStringLiteral("M1")),
        }));
    // Holder keys per euvrcwidget.cpp::buildHolderSection().
    groups.append(group(
        QStringLiteral("holder"),
        {
            textField(QStringLiteral("holder_name"), QStringLiteral("Holder name"), QStringLiteral("Marko Petrović")),
        }));
    return groups;
}

inline QList<LibreSCRS::AgentClient::FieldGroup> makePivMock()
{
    using namespace detail;
    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    // CHUID keys per pivwidget.cpp::chuidTranslationMap().
    groups.append(group(
        QStringLiteral("chuid"),
        {
            textField(QStringLiteral("guid"), QStringLiteral("GUID"),
                      QStringLiteral("00112233445566778899AABBCCDDEEFF")),
            textField(QStringLiteral("fascn"), QStringLiteral("FASC-N"), QStringLiteral("1234567890ABCDEF")),
            dateField(QStringLiteral("expirationDate"), QStringLiteral("Expiration"), QStringLiteral("2030-01-01")),
        }));
    // CCC keys per pivwidget.cpp::cccTranslationMap().
    groups.append(group(QStringLiteral("ccc"), {
                                                   textField(QStringLiteral("cardIdentifier"),
                                                             QStringLiteral("Card ID"), QStringLiteral("0102030405")),
                                               }));
    // Printed keys per pivwidget.cpp::printedTranslationMap().
    groups.append(
        group(QStringLiteral("printed"),
              {
                  textField(QStringLiteral("name"), QStringLiteral("Name"), QStringLiteral("Test User")),
                  textField(QStringLiteral("serialNumber"), QStringLiteral("Serial"), QStringLiteral("123456")),
                  textField(QStringLiteral("issuerId"), QStringLiteral("Issuer"), QStringLiteral("01234")),
              }));
    // Discovery keys per pivwidget.cpp::discoveryTranslationMap(). Without
    // this group the retranslate test never builds the discovery section, so
    // its title and field label sit outside every snapshot.
    groups.append(group(
        QStringLiteral("discovery"),
        {
            textField(QStringLiteral("pinPolicy"), QStringLiteral("PIN policy"), QStringLiteral("App PIN preferred")),
        }));
    return groups;
}

} // namespace librecelik::test::i18n::mock
