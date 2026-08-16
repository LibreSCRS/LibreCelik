// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include <QList>
#include "pivtextdocument.h"
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

QList<FieldGroup> makePIVGroups()
{
    QList<FieldGroup> groups;

    auto addText = [](FieldGroup& g, const QString& key, const QString& val) {
        if (!val.isEmpty())
            g.fields.append(textField(key, val));
    };

    {
        FieldGroup chuid;
        chuid.key = QStringLiteral("chuid");
        addText(chuid, QStringLiteral("guid"), QStringLiteral("3F2504E0-4F89-11D3-9A0C-0305E82C3301"));
        addText(chuid, QStringLiteral("fascn"), QStringLiteral("1234567890ABCDEF"));
        addText(chuid, QStringLiteral("expirationDate"), QStringLiteral("2030-12-31"));
        groups.append(std::move(chuid));
    }
    {
        FieldGroup ccc;
        ccc.key = QStringLiteral("ccc");
        addText(ccc, QStringLiteral("cardIdentifier"), QStringLiteral("ABCDEF1234567890"));
        groups.append(std::move(ccc));
    }
    {
        FieldGroup printed;
        printed.key = QStringLiteral("printed");
        addText(printed, QStringLiteral("name"), QStringLiteral("John Doe"));
        addText(printed, QStringLiteral("employeeAffiliation"), QStringLiteral("Government"));
        addText(printed, QStringLiteral("org1"), QStringLiteral("Department of Testing"));
        addText(printed, QStringLiteral("org2"), QStringLiteral("Division of Units"));
        addText(printed, QStringLiteral("serialNumber"), QStringLiteral("SN-12345"));
        addText(printed, QStringLiteral("issuerId"), QStringLiteral("ISS-001"));
        groups.append(std::move(printed));
    }
    {
        FieldGroup discovery;
        discovery.key = QStringLiteral("discovery");
        addText(discovery, QStringLiteral("pinPolicy"), QStringLiteral("Application PIN required"));
        groups.append(std::move(discovery));
    }
    {
        FieldGroup keyHistory;
        keyHistory.key = QStringLiteral("key_history");
        addText(keyHistory, QStringLiteral("onCardCerts"), QStringLiteral("3"));
        addText(keyHistory, QStringLiteral("offCardCerts"), QStringLiteral("0"));
        addText(keyHistory, QStringLiteral("offCardURL"), QStringLiteral("https://example.com/certs"));
        groups.append(std::move(keyHistory));
    }

    return groups;
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
    auto data = makePIVGroups();
    EXPECT_NO_THROW(PIVTextDocument doc(data));
}

TEST_F(PIVTextDocumentTest, EmptyDataProducesValidDocument)
{
    QList<FieldGroup> emptyData;
    EXPECT_NO_THROW(PIVTextDocument doc(emptyData));
}
