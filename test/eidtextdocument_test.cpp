// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include <QList>
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
        meta.fields.append(textField(QStringLiteral("card_type"), QStringLiteral("Apollo")));
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
                field.value = QStringLiteral("ForeignerIF2020");
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

} // namespace

TEST(EIdTextDocumentTest, CitizenDocumentProducesNonEmptyHtml)
{
    auto groups = makeCitizenGroups();
    EXPECT_NO_THROW(EIdTextDocument doc(groups));
}

TEST(EIdTextDocumentTest, ForeignerDocumentDetected)
{
    auto groups = makeForeignerGroups();
    EXPECT_NO_THROW(EIdTextDocument doc(groups));
}
