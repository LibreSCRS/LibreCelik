// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include "pivtextdocument.h"
#include <LibreSCRS/Plugin/CardData.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {

LibreSCRS::Plugin::CardData makePIVCardData()
{
    LibreSCRS::Plugin::CardData data;
    data.cardType = "piv";

    auto addText = [](LibreSCRS::Plugin::CardFieldGroup& g, const std::string& key, const std::string& val) {
        if (!val.empty())
            g.fields.push_back({key, key, LibreSCRS::Plugin::FieldType::Text, {val.begin(), val.end()}});
    };

    {
        LibreSCRS::Plugin::CardFieldGroup chuid;
        chuid.groupKey = "chuid";
        addText(chuid, "guid", "3F2504E0-4F89-11D3-9A0C-0305E82C3301");
        addText(chuid, "fascn", "1234567890ABCDEF");
        addText(chuid, "expirationDate", "2030-12-31");
        data.groups.push_back(std::move(chuid));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup ccc;
        ccc.groupKey = "ccc";
        addText(ccc, "cardIdentifier", "ABCDEF1234567890");
        data.groups.push_back(std::move(ccc));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup printed;
        printed.groupKey = "printed";
        addText(printed, "name", "John Doe");
        addText(printed, "employeeAffiliation", "Government");
        addText(printed, "org1", "Department of Testing");
        addText(printed, "org2", "Division of Units");
        addText(printed, "serialNumber", "SN-12345");
        addText(printed, "issuerId", "ISS-001");
        data.groups.push_back(std::move(printed));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup discovery;
        discovery.groupKey = "discovery";
        addText(discovery, "pinPolicy", "Application PIN required");
        data.groups.push_back(std::move(discovery));
    }
    {
        LibreSCRS::Plugin::CardFieldGroup keyHistory;
        keyHistory.groupKey = "key_history";
        addText(keyHistory, "onCardCerts", "3");
        addText(keyHistory, "offCardCerts", "0");
        addText(keyHistory, "offCardURL", "https://example.com/certs");
        data.groups.push_back(std::move(keyHistory));
    }

    return data;
}

} // namespace

class PIVTextDocumentTest : public ::testing::Test
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
QApplication* PIVTextDocumentTest::app = nullptr;

TEST_F(PIVTextDocumentTest, ConstructionSucceeds)
{
    auto data = makePIVCardData();
    EXPECT_NO_THROW(PIVTextDocument doc(data));
}

TEST_F(PIVTextDocumentTest, EmptyDataProducesValidDocument)
{
    LibreSCRS::Plugin::CardData emptyData;
    emptyData.cardType = "piv";
    EXPECT_NO_THROW(PIVTextDocument doc(emptyData));
}
