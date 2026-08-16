// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include <QList>
#include "euvrctextdocument.h"
#include <LibreSCRS/AgentClient/Types.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {

using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

// Test helper: subclass that exposes the protected QTextDocument for assertions
class TestableEuVrcTextDocument : public EuVrcTextDocument
{
public:
    using EuVrcTextDocument::EuVrcTextDocument;
    QString toHtml() const
    {
        return document.toHtml();
    }
};

/// A text field in the shape the agent's identity read ships: the value is
/// already stringified, the label fallback is the key (as the outgoing
/// fixtures spelled it), and the `type` token is the one the shared flatten
/// rule reads.
void addText(FieldGroup& group, const QString& key, const QString& value)
{
    if (value.isEmpty())
        return;
    Field field;
    field.key = key;
    field.value = value;
    field.extra.insert(QStringLiteral("labelFallback"), key);
    field.extra.insert(QStringLiteral("type"), QStringLiteral("text"));
    group.fields.append(field);
}

QList<FieldGroup> makeFullCardGroups()
{
    QList<FieldGroup> groups;

    {
        FieldGroup reg;
        reg.key = QStringLiteral("registration");
        addText(reg, QStringLiteral("registration_number"), QStringLiteral("BG 123-AB"));
        addText(reg, QStringLiteral("date_of_first_registration"), QStringLiteral("01.01.2020"));
        addText(reg, QStringLiteral("registration_date"), QStringLiteral("15.06.2023"));
        addText(reg, QStringLiteral("expiry_date"), QStringLiteral("01.01.2024"));
        addText(reg, QStringLiteral("document_number"), QStringLiteral("ABC123456"));
        addText(reg, QStringLiteral("issuing_authority"), QStringLiteral("MUP RS"));
        addText(reg, QStringLiteral("competent_authority"), QStringLiteral("MUP RS Beograd"));
        addText(reg, QStringLiteral("member_state"), QStringLiteral("SRB"));
        addText(reg, QStringLiteral("type_approval_number"), QStringLiteral("e1*2007/46*0001"));
        groups.append(std::move(reg));
    }
    {
        FieldGroup veh;
        veh.key = QStringLiteral("vehicle");
        addText(veh, QStringLiteral("vehicle_make"), QStringLiteral("VOLKSWAGEN"));
        addText(veh, QStringLiteral("vehicle_type"), QStringLiteral("GOLF"));
        addText(veh, QStringLiteral("commercial_description"), QStringLiteral("GOLF VII 2.0 TDI"));
        addText(veh, QStringLiteral("vehicle_id_number"), QStringLiteral("WVWZZZ1JZXW000001"));
        addText(veh, QStringLiteral("vehicle_category"), QStringLiteral("M1"));
        addText(veh, QStringLiteral("colour"), QStringLiteral("WHITE"));
        addText(veh, QStringLiteral("engine_capacity"), QStringLiteral("1968"));
        addText(veh, QStringLiteral("maximum_net_power"), QStringLiteral("110"));
        addText(veh, QStringLiteral("type_of_fuel"), QStringLiteral("DIESEL"));
        addText(veh, QStringLiteral("vehicle_mass"), QStringLiteral("1350"));
        addText(veh, QStringLiteral("maximum_permissible_laden_mass"), QStringLiteral("1880"));
        addText(veh, QStringLiteral("number_of_seats"), QStringLiteral("5"));
        groups.append(std::move(veh));
    }
    {
        FieldGroup holder;
        holder.key = QStringLiteral("holder");
        addText(holder, QStringLiteral("holder_name"), QStringLiteral("PETROVIC"));
        addText(holder, QStringLiteral("holder_other_names"), QStringLiteral("MARKO"));
        addText(holder, QStringLiteral("holder_address"),
                QStringLiteral("BEOGRAD,NOVI BEOGRAD,BULEVAR MIHAJLA PUPINA,207,,"));
        groups.append(std::move(holder));
    }
    {
        FieldGroup user;
        user.key = QStringLiteral("user");
        addText(user, QStringLiteral("user_name"), QStringLiteral("PETROVIC"));
        addText(user, QStringLiteral("user_other_names"), QStringLiteral("MARKO"));
        groups.append(std::move(user));
    }

    return groups;
}

QList<FieldGroup> makeCardGroupsWithNational()
{
    auto groups = makeFullCardGroups();

    FieldGroup nat;
    nat.key = QStringLiteral("national");
    addText(nat, QStringLiteral("owners_personal_no"), QStringLiteral("1234567890123"));
    addText(nat, QStringLiteral("year_of_production"), QStringLiteral("2019"));
    addText(nat, QStringLiteral("vehicle_load"), QStringLiteral("500"));
    groups.append(std::move(nat));

    return groups;
}

} // namespace

TEST(EuVrcTextDocumentTest, ConstructionSucceeds)
{
    auto data = makeFullCardGroups();
    EXPECT_NO_THROW({ EuVrcTextDocument doc(data); });
}

TEST(EuVrcTextDocumentTest, EmptyDataDoesNotCrash)
{
    QList<FieldGroup> empty;
    EXPECT_NO_THROW({ EuVrcTextDocument doc(empty); });
}

TEST(EuVrcTextDocumentTest, RegistrationOnlyDoesNotCrash)
{
    QList<FieldGroup> data;

    FieldGroup reg;
    reg.key = QStringLiteral("registration");
    addText(reg, QStringLiteral("registration_number"), QStringLiteral("BG 123"));
    data.append(std::move(reg));

    EXPECT_NO_THROW({ EuVrcTextDocument doc(data); });
}

TEST(EuVrcTextDocumentTest, EmptyFieldsNotInOutput)
{
    QList<FieldGroup> data;

    FieldGroup veh;
    veh.key = QStringLiteral("vehicle");
    addText(veh, QStringLiteral("vehicle_make"), QStringLiteral("FORD"));
    // vehicle_type intentionally omitted — should not appear in output
    data.append(std::move(veh));

    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();
    EXPECT_TRUE(html.contains("FORD"));
    // Check that the Type label does not appear since vehicle_type was not provided
    EXPECT_FALSE(html.contains("(D.2)"));
}

TEST(EuVrcTextDocumentTest, AddressCleanedInOutput)
{
    auto data = makeFullCardGroups();
    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();

    // The cleaned address should appear (comma-space separated, no trailing commas)
    EXPECT_TRUE(html.contains("BEOGRAD, NOVI BEOGRAD, BULEVAR MIHAJLA PUPINA, 207"));
    // Raw address with trailing commas should NOT appear
    EXPECT_FALSE(html.contains("207,,"));
}

TEST(EuVrcTextDocumentTest, NationalExtensionsAppear)
{
    auto data = makeCardGroupsWithNational();
    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();

    EXPECT_TRUE(html.contains("1234567890123"));
    EXPECT_TRUE(html.contains("2019"));
}

TEST(EuVrcTextDocumentTest, NewEuFieldsAppearWhenPopulated)
{
    QList<FieldGroup> data;

    FieldGroup veh;
    veh.key = QStringLiteral("vehicle");
    addText(veh, QStringLiteral("vehicle_make"), QStringLiteral("BMW"));
    addText(veh, QStringLiteral("co2_emissions"), QStringLiteral("CO2_TEST_VALUE_120g"));
    addText(veh, QStringLiteral("fuel_tank_capacity"), QStringLiteral("TANK_TEST_55L"));
    data.append(std::move(veh));

    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();

    EXPECT_TRUE(html.contains("CO2_TEST_VALUE_120g"));
    EXPECT_TRUE(html.contains("TANK_TEST_55L"));
}

TEST(EuVrcTextDocumentTest, ExpiredDateHighlighted)
{
    QList<FieldGroup> data;

    FieldGroup reg;
    reg.key = QStringLiteral("registration");
    addText(reg, QStringLiteral("registration_number"), QStringLiteral("BG 999-ZZ"));
    addText(reg, QStringLiteral("expiry_date"), QStringLiteral("01.01.2020")); // expired date
    data.append(std::move(reg));

    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();

    // QTextDocument converts class="expired" to inline style with the orange color
    // from euvrccard.css (.expired { color: #e65100; font-weight: bold; })
    // Check for the orange color or bold weight applied to the expired date
    EXPECT_TRUE(html.contains("#e65100") || html.contains("font-weight:600") || html.contains("expired"));
}
