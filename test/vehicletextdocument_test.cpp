// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include "vehicletextdocument.h"
#include <plugin/card_data.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {

plugin::CardData makeVehicleCardData()
{
    plugin::CardData data;
    data.cardType = "vehicle";

    auto addText = [](plugin::CardFieldGroup& g, const std::string& key, const std::string& val) {
        if (!val.empty())
            g.fields.push_back({key, key, plugin::FieldType::Text, {val.begin(), val.end()}});
    };

    {
        plugin::CardFieldGroup veh;
        veh.groupKey = "vehicle";
        addText(veh, "registration_number", "BG 123-AB");
        addText(veh, "vehicle_make", "VOLKSWAGEN");
        addText(veh, "vehicle_type", "GOLF");
        addText(veh, "vehicle_id_number", "WVWZZZ1JZXW000001");
        addText(veh, "issuing_date", "01.01.2023");
        addText(veh, "expiry_date", "01.01.2024");
        addText(veh, "engine_capacity", "1968");
        addText(veh, "vehicle_mass", "1350");
        data.groups.push_back(std::move(veh));
    }
    {
        plugin::CardFieldGroup owner;
        owner.groupKey = "owner";
        addText(owner, "owners_surname_or_business_name", "PETROVIĆ");
        addText(owner, "owner_name", "MARKO");
        addText(owner, "owner_address", "Knez Mihailova 10, Beograd");
        addText(owner, "owners_personal_no", "0101990710123");
        data.groups.push_back(std::move(owner));
    }
    {
        plugin::CardFieldGroup user;
        user.groupKey = "user";
        addText(user, "users_surname_or_business_name", "PETROVIĆ");
        addText(user, "users_name", "MARKO");
        data.groups.push_back(std::move(user));
    }

    return data;
}

} // namespace

TEST(VehicleTextDocumentTest, ConstructionSucceeds)
{
    auto data = makeVehicleCardData();
    EXPECT_NO_THROW(VehicleTextDocument doc(data));
}
