// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include <QList>
#include "healthtextdocument.h"
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

QList<FieldGroup> makeHealthGroups()
{
    QList<FieldGroup> groups;

    auto addText = [](FieldGroup& g, const QString& key, const QString& val) {
        if (!val.isEmpty())
            g.fields.append(textField(key, val));
    };

    {
        FieldGroup personal;
        personal.key = QStringLiteral("personal");
        addText(personal, QStringLiteral("given_name"), QStringLiteral("МАРКО"));
        addText(personal, QStringLiteral("family_name"), QStringLiteral("ПЕТРОВИЋ"));
        addText(personal, QStringLiteral("given_name_latin"), QStringLiteral("MARKO"));
        addText(personal, QStringLiteral("family_name_latin"), QStringLiteral("PETROVIĆ"));
        addText(personal, QStringLiteral("parent_name"), QStringLiteral("ИВАН"));
        addText(personal, QStringLiteral("date_of_birth"), QStringLiteral("01.01.1990"));
        addText(personal, QStringLiteral("gender"), QStringLiteral("M"));
        addText(personal, QStringLiteral("personal_number"), QStringLiteral("0101990710123"));
        addText(personal, QStringLiteral("insurant_number"), QStringLiteral("12345678901"));
        groups.append(std::move(personal));
    }
    {
        FieldGroup insurance;
        insurance.key = QStringLiteral("insurance");
        addText(insurance, QStringLiteral("insurer_name"), QStringLiteral("RFZO"));
        addText(insurance, QStringLiteral("insurer_id"), QStringLiteral("001"));
        addText(insurance, QStringLiteral("card_id"), QStringLiteral("ABC123"));
        addText(insurance, QStringLiteral("date_of_issue"), QStringLiteral("01.01.2023"));
        addText(insurance, QStringLiteral("date_of_expiry"), QStringLiteral("01.01.2028"));
        groups.append(std::move(insurance));
    }
    {
        FieldGroup address;
        address.key = QStringLiteral("address");
        addText(address, QStringLiteral("street"), QStringLiteral("Knez Mihailova"));
        addText(address, QStringLiteral("address_number"), QStringLiteral("10"));
        addText(address, QStringLiteral("place"), QStringLiteral("Beograd"));
        addText(address, QStringLiteral("municipality"), QStringLiteral("Stari Grad"));
        addText(address, QStringLiteral("country"), QStringLiteral("SRB"));
        groups.append(std::move(address));
    }
    {
        FieldGroup taxpayer;
        taxpayer.key = QStringLiteral("taxpayer");
        addText(taxpayer, QStringLiteral("taxpayer_name"), QStringLiteral("КОМПАНИЈА ДОО"));
        addText(taxpayer, QStringLiteral("taxpayer_id_number"), QStringLiteral("123456789"));
        groups.append(std::move(taxpayer));
    }

    return groups;
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
    auto data = makeHealthGroups();
    EXPECT_NO_THROW(HealthTextDocument doc(data));
}
