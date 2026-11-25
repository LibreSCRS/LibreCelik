// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include <QList>
#include <QTranslator>
#include <LibreSCRS/AgentClient/Types.h>
#include "eidtextdocument.h"

// QTextDocument requires QApplication
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {

using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

/// A text field in the shape the agent's identity read ships: the value is
/// already stringified, and the `type` token is the one the shared flatten
/// rule reads.
Field textField(const QString& key, const QString& value)
{
    Field field;
    field.key = key;
    field.value = value;
    field.extra.insert(QStringLiteral("type"), QStringLiteral("text"));
    return field;
}

QList<FieldGroup> makeCitizenGroups()
{
    QList<FieldGroup> groups;

    // meta group (card_type determines citizen vs foreigner)
    {
        FieldGroup meta;
        meta.key = QStringLiteral("meta");
        meta.fields.append(textField(QStringLiteral("card_type"), QStringLiteral("ID")));
        groups.append(std::move(meta));
    }
    // personal group
    {
        FieldGroup personal;
        personal.key = QStringLiteral("personal");
        personal.fields.append(textField(QStringLiteral("surname"), QStringLiteral("PETROVIĆ")));
        personal.fields.append(textField(QStringLiteral("given_name"), QStringLiteral("MARKO")));
        personal.fields.append(textField(QStringLiteral("parent_given_name"), QStringLiteral("IVAN")));
        personal.fields.append(textField(QStringLiteral("personal_number"), QStringLiteral("0101990710123")));
        personal.fields.append(textField(QStringLiteral("sex"), QStringLiteral("M")));
        personal.fields.append(textField(QStringLiteral("date_of_birth"), QStringLiteral("01.01.1990")));
        personal.fields.append(textField(QStringLiteral("place_of_birth"), QStringLiteral("Beograd")));
        personal.fields.append(textField(QStringLiteral("community_of_birth"), QStringLiteral("Stari Grad")));
        personal.fields.append(textField(QStringLiteral("state_of_birth"), QStringLiteral("SRB")));
        groups.append(std::move(personal));
    }
    // address group
    {
        FieldGroup address;
        address.key = QStringLiteral("address");
        address.fields.append(textField(QStringLiteral("street"), QStringLiteral("Knez Mihailova")));
        address.fields.append(textField(QStringLiteral("house_number"), QStringLiteral("10")));
        address.fields.append(textField(QStringLiteral("place"), QStringLiteral("Beograd")));
        address.fields.append(textField(QStringLiteral("community"), QStringLiteral("Stari Grad")));
        address.fields.append(textField(QStringLiteral("state"), QStringLiteral("SRB")));
        address.fields.append(textField(QStringLiteral("address_date"), QStringLiteral("15.03.2020")));
        groups.append(std::move(address));
    }
    // document group
    {
        FieldGroup document;
        document.key = QStringLiteral("document");
        document.fields.append(textField(QStringLiteral("doc_reg_no"), QStringLiteral("006953897")));
        document.fields.append(textField(QStringLiteral("issuing_date"), QStringLiteral("01.06.2020")));
        document.fields.append(textField(QStringLiteral("expiry_date"), QStringLiteral("01.06.2030")));
        document.fields.append(textField(QStringLiteral("issuing_authority"), QStringLiteral("PU Beograd")));
        groups.append(std::move(document));
    }

    return groups;
}

QList<FieldGroup> makeForeignerGroups()
{
    auto groups = makeCitizenGroups();
    // Change card_type to foreigner
    for (FieldGroup& group : groups) {
        if (group.key != QLatin1String("meta"))
            continue;
        for (Field& field : group.fields) {
            if (field.key == QLatin1String("card_type"))
                field.value = QStringLiteral("IF");
        }
    }
    // Add foreigner-specific fields
    for (FieldGroup& group : groups) {
        if (group.key != QLatin1String("personal"))
            continue;
        group.fields.append(textField(QStringLiteral("nationality"), QStringLiteral("German")));
        group.fields.append(textField(QStringLiteral("status_of_foreigner"), QStringLiteral("Stalno nastanjen")));
    }
    return groups;
}

void setCardType(QList<FieldGroup>& groups, const QString& code)
{
    for (FieldGroup& group : groups) {
        if (group.key != QLatin1String("meta"))
            continue;
        for (Field& field : group.fields) {
            if (field.key == QLatin1String("card_type"))
                field.value = code;
        }
    }
}

/// Residence permit as read from the 2026-11-25 test card (text values only).
QList<FieldGroup> makeResidencePermitGroups()
{
    auto groups = makeForeignerGroups();
    setCardType(groups, QStringLiteral("RP"));
    for (FieldGroup& group : groups) {
        if (group.key == QLatin1String("personal")) {
            group.fields.append(
                textField(QStringLiteral("purpose_of_stay"), QStringLiteral("KRETANJE U OKVIRU PRIVREDNOG DRUŠTVA")));
            group.fields.append(textField(QStringLiteral("note"), QStringLiteral("JEDINSTVENA DOZVOLA")));
        } else if (group.key == QLatin1String("document")) {
            group.fields.append(
                textField(QStringLiteral("document_name"), QStringLiteral("DOZVOLA ZA PRIVREMENI BORAVAK I RAD")));
            group.fields.append(textField(QStringLiteral("card_number"), QStringLiteral("BD00169609")));
        } else if (group.key == QLatin1String("address")) {
            // The placeholder date the card carries must print as unavailable.
            for (Field& field : group.fields) {
                if (field.key == QLatin1String("address_date"))
                    field.value = QStringLiteral("01.01.0001");
            }
        }
    }
    return groups;
}

/// Service ID: no address group, no expiry date.
QList<FieldGroup> makeServiceIdGroups()
{
    QList<FieldGroup> groups;
    FieldGroup meta;
    meta.key = QStringLiteral("meta");
    meta.fields.append(textField(QStringLiteral("card_type"), QStringLiteral("EL")));
    groups.append(std::move(meta));

    FieldGroup personal;
    personal.key = QStringLiteral("personal");
    personal.fields.append(textField(QStringLiteral("surname"), QStringLiteral("ШАЈЕРМАН")));
    personal.fields.append(textField(QStringLiteral("given_name"), QStringLiteral("ЈОВАНА")));
    personal.fields.append(textField(QStringLiteral("blood_group"), QStringLiteral("A-")));
    groups.append(std::move(personal));

    FieldGroup service;
    service.key = QStringLiteral("service");
    service.fields.append(textField(QStringLiteral("status"), QStringLiteral("1")));
    service.fields.append(textField(QStringLiteral("badge_number"), QStringLiteral("00000097")));
    service.fields.append(textField(QStringLiteral("rank"), QStringLiteral("Načelnik")));
    service.fields.append(textField(QStringLiteral("organisational_unit"), QStringLiteral("Personalizacija")));
    service.fields.append(textField(QStringLiteral("position"), QStringLiteral("Šef")));
    service.fields.append(textField(QStringLiteral("service_address"), QStringLiteral("Kneza Miloša 101")));
    groups.append(std::move(service));

    FieldGroup document;
    document.key = QStringLiteral("document");
    document.fields.append(textField(QStringLiteral("doc_reg_no"), QStringLiteral("400000097")));
    document.fields.append(textField(QStringLiteral("issuing_date"), QStringLiteral("20.11.2026")));
    document.fields.append(textField(QStringLiteral("issuing_authority"), QStringLiteral("MUP Republike Srbije")));
    groups.append(std::move(document));
    return groups;
}

/// Exposes the rendered text: these cases assert on what the printer is handed.
class TestableEIdTextDocument : public EIdTextDocument
{
public:
    using EIdTextDocument::EIdTextDocument;

    QString text() const
    {
        return document.toPlainText();
    }
};

/// Translated labels need a catalogue; without one qtTrId() hands back ids.
class EIdTextDocumentTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        translator = new QTranslator();
        ASSERT_TRUE(translator->load(QStringLiteral("LibreCelik_sr_Latn_RS"),
                                     QStringLiteral(LIBRECELIK_TRANSLATIONS_DIR_DEFAULT)));
        QCoreApplication::installTranslator(translator);
    }

    void TearDown() override
    {
        QCoreApplication::removeTranslator(translator);
        delete translator;
    }

    QTranslator* translator = nullptr;
};

} // namespace

TEST_F(EIdTextDocumentTest, CitizenDocumentProducesNonEmptyHtml)
{
    auto groups = makeCitizenGroups();
    EXPECT_NO_THROW(EIdTextDocument doc(groups));
}

TEST_F(EIdTextDocumentTest, ForeignerDocumentDetected)
{
    auto groups = makeForeignerGroups();
    EXPECT_NO_THROW(EIdTextDocument doc(groups));
    const TestableEIdTextDocument doc(groups);
    EXPECT_TRUE(doc.text().contains(QStringLiteral("Lična karta za stranca")));
    EXPECT_FALSE(doc.text().contains(QStringLiteral("Boravišna dozvola")));
}

TEST_F(EIdTextDocumentTest, ResidencePermitUsesItsTemplate)
{
    const auto groups = makeResidencePermitGroups();
    const TestableEIdTextDocument doc(groups);
    const QString text = doc.text();

    EXPECT_TRUE(text.contains(QStringLiteral("Boravišna dozvola")));
    EXPECT_FALSE(text.contains(QStringLiteral("Lična karta za stranca")));
    EXPECT_TRUE(text.contains(QStringLiteral("Naziv dokumenta:")));
    EXPECT_TRUE(text.contains(QStringLiteral("DOZVOLA ZA PRIVREMENI BORAVAK I RAD")));
    EXPECT_TRUE(text.contains(QStringLiteral("Svrha boravka:")));
    EXPECT_TRUE(text.contains(QStringLiteral("KRETANJE U OKVIRU PRIVREDNOG DRUŠTVA")));
    EXPECT_TRUE(text.contains(QStringLiteral("Napomena:")));
    EXPECT_TRUE(text.contains(QStringLiteral("JEDINSTVENA DOZVOLA")));
    EXPECT_TRUE(text.contains(QStringLiteral("Broj kartice:")));
    EXPECT_TRUE(text.contains(QStringLiteral("BD00169609")));
    EXPECT_FALSE(text.contains(QStringLiteral("01.01.0001"))) << "the placeholder address date must stay hidden";
    EXPECT_FALSE(text.contains(QStringLiteral("${"))) << "no template token may survive";
}

TEST_F(EIdTextDocumentTest, ServiceIdUsesItsTemplate)
{
    const auto groups = makeServiceIdGroups();
    const TestableEIdTextDocument doc(groups);
    const QString text = doc.text();

    EXPECT_TRUE(text.contains(QStringLiteral("Službena legitimacija")));
    EXPECT_FALSE(text.contains(QStringLiteral("Lična karta za stranca")));
    EXPECT_TRUE(text.contains(QStringLiteral("Zvanje:")));
    EXPECT_TRUE(text.contains(QStringLiteral("Načelnik")));
    EXPECT_TRUE(text.contains(QStringLiteral("Broj službene značke:")));
    EXPECT_TRUE(text.contains(QStringLiteral("00000097")));
    EXPECT_TRUE(text.contains(QStringLiteral("Krvna grupa:")));
    EXPECT_TRUE(text.contains(QStringLiteral("A-")));
    EXPECT_TRUE(text.contains(QStringLiteral("Adresa službe:")));
    EXPECT_TRUE(text.contains(QStringLiteral("Kneza Miloša 101")));
    EXPECT_TRUE(text.contains(QStringLiteral("400000097")));
    EXPECT_FALSE(text.contains(QStringLiteral("Datum promene adrese"))) << "service ID has no address block";
    EXPECT_FALSE(text.contains(QStringLiteral("Važi do"))) << "service ID has no expiry date";
    EXPECT_FALSE(text.contains(QStringLiteral("${"))) << "no template token may survive";
}
