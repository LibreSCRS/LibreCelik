// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include "utils/fieldsectionbuilder.h"
#include <LibreSCRS/AgentClient/Types.h>

namespace {

using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

/// A text field carrying the agent's own row metadata. `labelFallback` is the
/// agent-authored English label the builder falls back to when the host has no
/// translation for the key; `type` is the token the shared flatten rule reads.
Field textField(const QString& key, const QString& value, const QString& labelFallback = {})
{
    Field field;
    field.key = key;
    field.value = value;
    field.extra.insert(QStringLiteral("type"), QStringLiteral("text"));
    if (!labelFallback.isEmpty())
        field.extra.insert(QStringLiteral("labelFallback"), labelFallback);
    return field;
}

} // namespace

class FieldSectionBuilderTest : public ::testing::Test
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
QApplication* FieldSectionBuilderTest::app = nullptr;

TEST_F(FieldSectionBuilderTest, CreatesReadOnlyFieldsFromGroup)
{
    FieldGroup group;
    group.key = QStringLiteral("personal");
    group.fields.append(textField(QStringLiteral("given_name"), QStringLiteral("Petar")));
    group.fields.append(textField(QStringLiteral("surname"), QStringLiteral("Petrovic")));

    auto* section = librecelik::utils::FieldSectionBuilder::build("Personal Data", group, {});

    ASSERT_NE(section, nullptr);
    auto edits = section->findChildren<QLineEdit*>();
    ASSERT_EQ(edits.size(), 2);
    QStringList values;
    for (auto* edit : edits) {
        EXPECT_TRUE(edit->isReadOnly());
        values << edit->text();
    }
    EXPECT_TRUE(values.contains("Petar"));
    EXPECT_TRUE(values.contains("Petrovic"));

    delete section;
}

TEST_F(FieldSectionBuilderTest, SkipsEmptyFields)
{
    FieldGroup group;
    group.key = QStringLiteral("test");
    group.fields.append(textField(QStringLiteral("filled"), QStringLiteral("A")));
    group.fields.append(textField(QStringLiteral("empty"), QString{}));

    auto* section = librecelik::utils::FieldSectionBuilder::build("Test", group, {});
    auto edits = section->findChildren<QLineEdit*>();
    ASSERT_EQ(edits.size(), 1);
    EXPECT_EQ(edits[0]->text(), "A");

    delete section;
}

// The skip-binary decision is not re-implemented here — it lives in the
// client's shared flatten rule, so a binary field (a raw portrait) never
// reaches the grid at all, exactly as it does not reach the KDE rows consumer.
TEST_F(FieldSectionBuilderTest, SkipsBinaryFields)
{
    FieldGroup group;
    group.key = QStringLiteral("personal");
    group.fields.append(textField(QStringLiteral("given_name"), QStringLiteral("Petar")));

    Field portrait;
    portrait.key = QStringLiteral("photo");
    portrait.value = QStringLiteral("ignored");
    portrait.extra.insert(QStringLiteral("type"), QStringLiteral("binary"));
    group.fields.append(portrait);

    auto* section = librecelik::utils::FieldSectionBuilder::build("Personal", group, {});
    auto edits = section->findChildren<QLineEdit*>();
    ASSERT_EQ(edits.size(), 1);
    EXPECT_EQ(edits[0]->text(), "Petar");

    delete section;
}

TEST_F(FieldSectionBuilderTest, UsesTranslationMap)
{
    FieldGroup group;
    group.key = QStringLiteral("personal");
    group.fields.append(textField(QStringLiteral("given_name"), QStringLiteral("P"), QStringLiteral("Given name")));

    std::map<QString, QString> translations = {{QStringLiteral("given_name"), QStringLiteral("Ime")}};

    auto* section = librecelik::utils::FieldSectionBuilder::build("Personal", group, translations);
    auto labels = section->findChildren<QLabel*>();
    bool found = false;
    for (auto* label : labels) {
        if (label->text() == "Ime")
            found = true;
    }
    EXPECT_TRUE(found);

    delete section;
}

// Second rung of the label precedence: no host translation, but the agent
// shipped an English label with the row.
TEST_F(FieldSectionBuilderTest, FallsBackToAgentLabelWhenNoTranslation)
{
    FieldGroup group;
    group.key = QStringLiteral("test");
    group.fields.append(textField(QStringLiteral("my_field"), QStringLiteral("X"), QStringLiteral("My Field")));

    auto* section = librecelik::utils::FieldSectionBuilder::build("Test", group, {});
    auto labels = section->findChildren<QLabel*>();
    bool found = false;
    for (auto* label : labels) {
        if (label->text() == "My Field")
            found = true;
    }
    EXPECT_TRUE(found);

    delete section;
}

TEST_F(FieldSectionBuilderTest, FallsBackToKeyWhenNoTranslation)
{
    FieldGroup group;
    group.key = QStringLiteral("test");
    group.fields.append(textField(QStringLiteral("my_field"), QStringLiteral("X")));

    auto* section = librecelik::utils::FieldSectionBuilder::build("Test", group, {});
    auto labels = section->findChildren<QLabel*>();
    bool found = false;
    for (auto* label : labels) {
        if (label->text() == "my_field")
            found = true;
    }
    EXPECT_TRUE(found);

    delete section;
}

TEST_F(FieldSectionBuilderTest, HidesFieldsInHiddenSet)
{
    FieldGroup group;
    group.key = QStringLiteral("personal");
    group.fields.append(textField(QStringLiteral("visible"), QStringLiteral("A")));
    group.fields.append(textField(QStringLiteral("hidden"), QStringLiteral("B")));

    std::set<QString> hidden = {QStringLiteral("hidden")};
    auto* section = librecelik::utils::FieldSectionBuilder::build("Test", group, {}, hidden);
    auto edits = section->findChildren<QLineEdit*>();
    ASSERT_EQ(edits.size(), 1);
    EXPECT_EQ(edits[0]->text(), "A");

    delete section;
}
