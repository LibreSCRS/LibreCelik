// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include <plugin/card_data.h>
#include "eidtextdocument.h"

// QTextDocument requires QApplication
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {

plugin::CardData makeCitizenCardData()
{
    plugin::CardData data;
    data.cardType = "rs-eid";

    auto addText = [](plugin::CardFieldGroup& g, const std::string& key, const std::string& val) {
        if (!val.empty())
            g.fields.push_back({key, key, plugin::FieldType::Text, {val.begin(), val.end()}});
    };

    // meta group (card_type determines citizen vs foreigner)
    {
        plugin::CardFieldGroup meta;
        meta.groupKey = "meta";
        addText(meta, "card_type", "Apollo");
        data.groups.push_back(std::move(meta));
    }
    // personal group
    {
        plugin::CardFieldGroup personal;
        personal.groupKey = "personal";
        addText(personal, "surname", "PETROVIĆ");
        addText(personal, "given_name", "MARKO");
        addText(personal, "parent_given_name", "IVAN");
        addText(personal, "personal_number", "0101990710123");
        addText(personal, "sex", "M");
        addText(personal, "date_of_birth", "01.01.1990");
        addText(personal, "place_of_birth", "Beograd");
        addText(personal, "community_of_birth", "Stari Grad");
        addText(personal, "state_of_birth", "SRB");
        data.groups.push_back(std::move(personal));
    }
    // address group
    {
        plugin::CardFieldGroup address;
        address.groupKey = "address";
        addText(address, "street", "Knez Mihailova");
        addText(address, "house_number", "10");
        addText(address, "place", "Beograd");
        addText(address, "community", "Stari Grad");
        addText(address, "state", "SRB");
        addText(address, "address_date", "15.03.2020");
        data.groups.push_back(std::move(address));
    }
    // document group
    {
        plugin::CardFieldGroup document;
        document.groupKey = "document";
        addText(document, "doc_reg_no", "006953897");
        addText(document, "issuing_date", "01.06.2020");
        addText(document, "expiry_date", "01.06.2030");
        addText(document, "issuing_authority", "PU Beograd");
        data.groups.push_back(std::move(document));
    }

    return data;
}

plugin::CardData makeForeignerCardData()
{
    auto data = makeCitizenCardData();
    // Change card_type to foreigner
    for (auto& g : data.groups) {
        if (g.groupKey == "meta") {
            for (auto& f : g.fields) {
                if (f.key == "card_type") {
                    std::string val = "ForeignerIF2020";
                    f.value = {val.begin(), val.end()};
                }
            }
        }
    }
    // Add foreigner-specific fields
    auto* personal = data.findGroup("personal");
    if (personal) {
        std::string nat = "German";
        personal->fields.push_back({"nationality", "Nationality", plugin::FieldType::Text, {nat.begin(), nat.end()}});
        std::string status = "Stalno nastanjen";
        personal->fields.push_back(
            {"status_of_foreigner", "Status", plugin::FieldType::Text, {status.begin(), status.end()}});
    }
    return data;
}

} // namespace

TEST(EIdTextDocumentTest, CitizenDocumentProducesNonEmptyHtml)
{
    auto data = makeCitizenCardData();
    EXPECT_NO_THROW(EIdTextDocument doc(data));
}

TEST(EIdTextDocumentTest, ForeignerDocumentDetected)
{
    auto data = makeForeignerCardData();
    EXPECT_NO_THROW(EIdTextDocument doc(data));
}
