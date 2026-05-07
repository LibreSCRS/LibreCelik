// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include "healthtextdocument.h"
#include <LibreSCRS/Plugin/CardData.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {

LibreSCRS::Plugin::CardData makeHealthCardData()
{
    LibreSCRS::Plugin::CardData data;
    data.cardType = "rs-health";

    auto addText = [](LibreSCRS::Plugin::CardFieldGroup& g, const std::string& key, const std::string& val) {
        if (!val.empty())
            g.fields.push_back({key, key, LibreSCRS::Plugin::FieldType::Text, {val.begin(), val.end()}});
    };

    {
        LibreSCRS::Plugin::CardFieldGroup personal;
        personal.groupKey = "personal";
        addText(personal, "given_name", "МАРКО");
        addText(personal, "family_name", "ПЕТРОВИЋ");
        addText(personal, "given_name_latin", "MARKO");
        addText(personal, "family_name_latin", "PETROVIĆ");
        addText(personal, "parent_name", "ИВАН");
        addText(personal, "date_of_birth", "01.01.1990");
        addText(personal, "gender", "M");
        addText(personal, "personal_number", "0101990710123");
        addText(personal, "insurant_number", "12345678901");
        data.groups.push_back(std::move(personal));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup insurance;
        insurance.groupKey = "insurance";
        addText(insurance, "insurer_name", "RFZO");
        addText(insurance, "insurer_id", "001");
        addText(insurance, "card_id", "ABC123");
        addText(insurance, "date_of_issue", "01.01.2023");
        addText(insurance, "date_of_expiry", "01.01.2028");
        data.groups.push_back(std::move(insurance));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup address;
        address.groupKey = "address";
        addText(address, "street", "Knez Mihailova");
        addText(address, "address_number", "10");
        addText(address, "place", "Beograd");
        addText(address, "municipality", "Stari Grad");
        addText(address, "country", "SRB");
        data.groups.push_back(std::move(address));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup taxpayer;
        taxpayer.groupKey = "taxpayer";
        addText(taxpayer, "taxpayer_name", "КОМПАНИЈА ДОО");
        addText(taxpayer, "taxpayer_id_number", "123456789");
        data.groups.push_back(std::move(taxpayer));
    }

    return data;
}

} // namespace

class HealthTextDocumentTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            int argc = 0;
            app = new QApplication(argc, nullptr);
        }
    }
    static QApplication* app;
};
QApplication* HealthTextDocumentTest::app = nullptr;

TEST_F(HealthTextDocumentTest, ConstructionSucceeds)
{
    auto data = makeHealthCardData();
    EXPECT_NO_THROW(HealthTextDocument doc(data));
}
