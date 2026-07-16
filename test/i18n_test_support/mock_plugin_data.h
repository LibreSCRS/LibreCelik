// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Synthetic CardData factories for D9 plugin retranslate tests.
///
/// Each `make<Plugin>Mock()` returns a minimal-but-complete
/// LibreSCRS::Plugin::CardData payload sufficient to populate every
/// section the corresponding plugin widget renders. No PCSC, no real
/// card hardware. Used by `i18n_plugin_retranslate_test.cpp` as
/// parameter sources.

#pragma once

#include <LibreSCRS/Plugin/CardData.h>

#include <cstdint>
#include <string>
#include <vector>

namespace librecelik::test::i18n::mock {

namespace detail {

inline LibreSCRS::Plugin::CardField textField(const std::string& key, const std::string& label,
                                              const std::string& value)
{
    LibreSCRS::Plugin::CardField f;
    f.key = key;
    f.label = label;
    f.type = LibreSCRS::Plugin::FieldType::Text;
    f.value.assign(value.begin(), value.end());
    return f;
}

inline LibreSCRS::Plugin::CardField dateField(const std::string& key, const std::string& label,
                                              const std::string& value)
{
    LibreSCRS::Plugin::CardField f;
    f.key = key;
    f.label = label;
    f.type = LibreSCRS::Plugin::FieldType::Date;
    f.value.assign(value.begin(), value.end());
    return f;
}

inline LibreSCRS::Plugin::CardFieldGroup group(const std::string& key, std::vector<LibreSCRS::Plugin::CardField> fields)
{
    LibreSCRS::Plugin::CardFieldGroup g;
    g.groupKey = key;
    g.fields = std::move(fields);
    return g;
}

} // namespace detail

// Field keys below are aligned with the production plugin widget
// translation maps (eidwidget.cpp, emrtdwidget.cpp, etc.). Mismatched
// keys fall through to a literal-key label rendering and would make the
// retranslate test's stable-label heuristic flag the field as a stale
// label after switching language; keep these aligned.

inline LibreSCRS::Plugin::CardData makeEidMock()
{
    using namespace detail;
    LibreSCRS::Plugin::CardData d;
    d.cardType = "rs-eid";
    // EidWidget reads "meta" first to build the outer section.
    d.groups.push_back(group("meta", {
                                         textField("card_type", "Card type", "Citizen2014"),
                                     }));
    // Personal group keys per eidwidget.cpp::buildPersonalSection().
    d.groups.push_back(group("personal", {
                                             textField("given_name", "Given name", "Marko"),
                                             textField("surname", "Surname", "Petrović"),
                                             dateField("date_of_birth", "Date of birth", "1990-01-01"),
                                             textField("personal_number", "JMBG", "1234567890123"),
                                             textField("sex", "Sex", "M"),
                                             textField("parent_given_name", "Parent name", "Petar"),
                                         }));
    // Address keys per eidwidget.cpp::buildAddressSection().
    d.groups.push_back(group("address", {
                                            textField("place", "Place", "Beograd"),
                                            textField("street", "Street", "Knez Mihailova"),
                                            textField("house_number", "Number", "1"),
                                        }));
    // Document keys per eidwidget.cpp::buildDocumentSection().
    d.groups.push_back(group("document", {
                                             textField("document_type", "Document type", "ID"),
                                             textField("document_serial_number", "Serial", "123456789"),
                                             textField("issuing_authority", "Issuer", "MUP"),
                                             dateField("expiry_date", "Expiry", "2030-01-01"),
                                         }));
    return d;
}

inline LibreSCRS::Plugin::CardData makeHealthMock()
{
    using namespace detail;
    LibreSCRS::Plugin::CardData d;
    d.cardType = "rs-health";
    // Personal keys per healthwidget.cpp::addPersonalGroup().
    d.groups.push_back(group("personal", {
                                             textField("given_name", "Given name", "Ana"),
                                             textField("family_name", "Family name", "Jovanović"),
                                             textField("personal_number", "JMBG", "1234567890123"),
                                             textField("insurant_number", "LBO", "987654321"),
                                         }));
    // Insurance keys per healthwidget.cpp::insuranceTranslationMap().
    d.groups.push_back(group("insurance", {
                                              textField("insurer_name", "Insurer", "RFZO"),
                                              textField("insurer_id", "Insurer ID", "01"),
                                              dateField("date_of_expiry", "Expiry", "2026-12-31"),
                                              // Exercises HealthWidget::transformPermanentlyValid (true -> Yes/Да)
                                              // across a language switch: regression guard that the render-only
                                              // transform + retranslate rebuild never mutate the source-of-truth.
                                              textField("permanently_valid", "Permanently valid", "true"),
                                          }));
    // Address keys per healthwidget.cpp::addressTranslationMap().
    d.groups.push_back(group("address", {
                                            textField("street", "Street", "Knez Mihailova"),
                                            textField("place", "Place", "Beograd"),
                                        }));
    return d;
}

inline LibreSCRS::Plugin::CardData makeEmrtdMock()
{
    using namespace detail;
    LibreSCRS::Plugin::CardData d;
    d.cardType = "emrtd";
    // Personal keys per emrtdwidget.cpp::personalTranslationMap().
    d.groups.push_back(group("personal", {
                                             textField("given_names", "Given names", "John"),
                                             textField("surname", "Surname", "Doe"),
                                             textField("nationality", "Nationality", "USA"),
                                             dateField("date_of_birth", "Date of birth", "1990-01-01"),
                                         }));
    // Document keys per emrtdwidget.cpp::documentTranslationMap().
    d.groups.push_back(group("document", {
                                             textField("document_number", "Document number", "X1234567"),
                                             textField("document_code", "Code", "P"),
                                             textField("issuing_state", "Issuing state", "USA"),
                                             dateField("date_of_expiry", "Expiry", "2030-01-01"),
                                         }));
    return d;
}

inline LibreSCRS::Plugin::CardData makeEuVrcMock()
{
    using namespace detail;
    LibreSCRS::Plugin::CardData d;
    d.cardType = "eu-vrc";
    // Registration keys per euvrcwidget.cpp::buildRegistrationSection().
    d.groups.push_back(group("registration", {
                                                 textField("registration_number", "Reg number", "BG-123-AA"),
                                                 dateField("expiry_date", "Expiry", "2030-01-01"),
                                                 textField("member_state", "Member state", "RS"),
                                                 textField("issuing_authority", "Issuing authority", "MUP"),
                                             }));
    // Vehicle keys per euvrcwidget.cpp::buildVehicleSection().
    d.groups.push_back(group("vehicle", {
                                            textField("vehicle_make", "Make", "VW"),
                                            textField("vehicle_id_number", "VIN", "WAUZZZ8K3AA000000"),
                                            textField("vehicle_category", "Category", "M1"),
                                        }));
    // Holder keys per euvrcwidget.cpp::buildHolderSection().
    d.groups.push_back(group("holder", {
                                           textField("holder_name", "Holder name", "Marko Petrović"),
                                       }));
    return d;
}

inline LibreSCRS::Plugin::CardData makePivMock()
{
    using namespace detail;
    LibreSCRS::Plugin::CardData d;
    d.cardType = "piv";
    // CHUID keys per pivwidget.cpp::chuidTranslationMap().
    d.groups.push_back(group("chuid", {
                                          textField("guid", "GUID", "00112233445566778899AABBCCDDEEFF"),
                                          textField("fascn", "FASC-N", "1234567890ABCDEF"),
                                          dateField("expirationDate", "Expiration", "2030-01-01"),
                                      }));
    // CCC keys per pivwidget.cpp::cccTranslationMap().
    d.groups.push_back(group("ccc", {
                                        textField("cardIdentifier", "Card ID", "0102030405"),
                                    }));
    // Printed keys per pivwidget.cpp::printedTranslationMap().
    d.groups.push_back(group("printed", {
                                            textField("name", "Name", "Test User"),
                                            textField("serialNumber", "Serial", "123456"),
                                            textField("issuerId", "Issuer", "01234"),
                                        }));
    return d;
}

} // namespace librecelik::test::i18n::mock
