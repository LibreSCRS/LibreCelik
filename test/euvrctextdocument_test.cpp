// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include "euvrctextdocument.h"
#include <LibreSCRS/Plugin/CardData.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {

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

auto addText = [](LibreSCRS::Plugin::CardFieldGroup& g, const std::string& key, const std::string& val) {
    if (!val.empty())
        g.fields.push_back({key, key, LibreSCRS::Plugin::FieldType::Text, {val.begin(), val.end()}});
};

LibreSCRS::Plugin::CardData makeFullCardData()
{
    LibreSCRS::Plugin::CardData data;
    data.cardType = "eu-vrc";

    {
        LibreSCRS::Plugin::CardFieldGroup reg;
        reg.groupKey = "registration";
        addText(reg, "registration_number", "BG 123-AB");
        addText(reg, "date_of_first_registration", "01.01.2020");
        addText(reg, "registration_date", "15.06.2023");
        addText(reg, "expiry_date", "01.01.2024");
        addText(reg, "document_number", "ABC123456");
        addText(reg, "issuing_authority", "MUP RS");
        addText(reg, "competent_authority", "MUP RS Beograd");
        addText(reg, "member_state", "SRB");
        addText(reg, "type_approval_number", "e1*2007/46*0001");
        data.groups.push_back(std::move(reg));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup veh;
        veh.groupKey = "vehicle";
        addText(veh, "vehicle_make", "VOLKSWAGEN");
        addText(veh, "vehicle_type", "GOLF");
        addText(veh, "commercial_description", "GOLF VII 2.0 TDI");
        addText(veh, "vehicle_id_number", "WVWZZZ1JZXW000001");
        addText(veh, "vehicle_category", "M1");
        addText(veh, "colour", "WHITE");
        addText(veh, "engine_capacity", "1968");
        addText(veh, "maximum_net_power", "110");
        addText(veh, "type_of_fuel", "DIESEL");
        addText(veh, "vehicle_mass", "1350");
        addText(veh, "maximum_permissible_laden_mass", "1880");
        addText(veh, "number_of_seats", "5");
        data.groups.push_back(std::move(veh));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup holder;
        holder.groupKey = "holder";
        addText(holder, "holder_name", "PETROVIC");
        addText(holder, "holder_other_names", "MARKO");
        addText(holder, "holder_address", "BEOGRAD,NOVI BEOGRAD,BULEVAR MIHAJLA PUPINA,207,,");
        data.groups.push_back(std::move(holder));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup user;
        user.groupKey = "user";
        addText(user, "user_name", "PETROVIC");
        addText(user, "user_other_names", "MARKO");
        data.groups.push_back(std::move(user));
    }

    return data;
}

LibreSCRS::Plugin::CardData makeCardDataWithNational()
{
    auto data = makeFullCardData();

    LibreSCRS::Plugin::CardFieldGroup nat;
    nat.groupKey = "national";
    addText(nat, "owners_personal_no", "1234567890123");
    addText(nat, "year_of_production", "2019");
    addText(nat, "vehicle_load", "500");
    data.groups.push_back(std::move(nat));

    return data;
}

} // namespace

TEST(EuVrcTextDocumentTest, ConstructionSucceeds)
{
    auto data = makeFullCardData();
    EXPECT_NO_THROW({ EuVrcTextDocument doc(data); });
}

TEST(EuVrcTextDocumentTest, EmptyDataDoesNotCrash)
{
    LibreSCRS::Plugin::CardData empty;
    empty.cardType = "eu-vrc";
    EXPECT_NO_THROW({ EuVrcTextDocument doc(empty); });
}

TEST(EuVrcTextDocumentTest, RegistrationOnlyDoesNotCrash)
{
    LibreSCRS::Plugin::CardData data;
    data.cardType = "eu-vrc";

    LibreSCRS::Plugin::CardFieldGroup reg;
    reg.groupKey = "registration";
    reg.fields.push_back({"registration_number",
                          "registration_number",
                          LibreSCRS::Plugin::FieldType::Text,
                          {'B', 'G', ' ', '1', '2', '3'}});
    data.groups.push_back(std::move(reg));

    EXPECT_NO_THROW({ EuVrcTextDocument doc(data); });
}

TEST(EuVrcTextDocumentTest, EmptyFieldsNotInOutput)
{
    LibreSCRS::Plugin::CardData data;
    data.cardType = "eu-vrc";

    LibreSCRS::Plugin::CardFieldGroup veh;
    veh.groupKey = "vehicle";
    addText(veh, "vehicle_make", "FORD");
    // vehicle_type intentionally omitted — should not appear in output
    data.groups.push_back(std::move(veh));

    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();
    EXPECT_TRUE(html.contains("FORD"));
    // Check that the Type label does not appear since vehicle_type was not provided
    EXPECT_FALSE(html.contains("(D.2)"));
}

TEST(EuVrcTextDocumentTest, AddressCleanedInOutput)
{
    auto data = makeFullCardData();
    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();

    // The cleaned address should appear (comma-space separated, no trailing commas)
    EXPECT_TRUE(html.contains("BEOGRAD, NOVI BEOGRAD, BULEVAR MIHAJLA PUPINA, 207"));
    // Raw address with trailing commas should NOT appear
    EXPECT_FALSE(html.contains("207,,"));
}

TEST(EuVrcTextDocumentTest, NationalExtensionsAppear)
{
    auto data = makeCardDataWithNational();
    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();

    EXPECT_TRUE(html.contains("1234567890123"));
    EXPECT_TRUE(html.contains("2019"));
}

TEST(EuVrcTextDocumentTest, NewEuFieldsAppearWhenPopulated)
{
    LibreSCRS::Plugin::CardData data;
    data.cardType = "eu-vrc";

    LibreSCRS::Plugin::CardFieldGroup veh;
    veh.groupKey = "vehicle";
    addText(veh, "vehicle_make", "BMW");
    addText(veh, "co2_emissions", "CO2_TEST_VALUE_120g");
    addText(veh, "fuel_tank_capacity", "TANK_TEST_55L");
    data.groups.push_back(std::move(veh));

    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();

    EXPECT_TRUE(html.contains("CO2_TEST_VALUE_120g"));
    EXPECT_TRUE(html.contains("TANK_TEST_55L"));
}

TEST(EuVrcTextDocumentTest, ExpiredDateHighlighted)
{
    LibreSCRS::Plugin::CardData data;
    data.cardType = "eu-vrc";

    LibreSCRS::Plugin::CardFieldGroup reg;
    reg.groupKey = "registration";
    addText(reg, "registration_number", "BG 999-ZZ");
    addText(reg, "expiry_date", "01.01.2020"); // expired date
    data.groups.push_back(std::move(reg));

    TestableEuVrcTextDocument doc(data);
    auto html = doc.toHtml();

    // QTextDocument converts class="expired" to inline style with the orange color
    // from euvrccard.css (.expired { color: #e65100; font-weight: bold; })
    // Check for the orange color or bold weight applied to the expired date
    EXPECT_TRUE(html.contains("#e65100") || html.contains("font-weight:600") || html.contains("expired"));
}
