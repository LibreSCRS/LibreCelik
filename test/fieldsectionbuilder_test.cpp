// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include "utils/fieldsectionbuilder.h"
#include <LibreSCRS/Plugin/CardData.h>

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
    LibreSCRS::Plugin::CardFieldGroup group;
    group.groupKey = "personal";
    group.fields.push_back({"given_name", "Given Name", LibreSCRS::Plugin::FieldType::Text, {'P', 'e', 't', 'a', 'r'}});
    group.fields.push_back(
        {"surname", "Surname", LibreSCRS::Plugin::FieldType::Text, {'P', 'e', 't', 'r', 'o', 'v', 'i', 'c'}});

    auto* section = LibreSCRS::FieldSectionBuilder::build("Personal Data", group, {});

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
    LibreSCRS::Plugin::CardFieldGroup group;
    group.groupKey = "test";
    group.fields.push_back({"filled", "Filled", LibreSCRS::Plugin::FieldType::Text, {'A'}});
    group.fields.push_back({"empty", "Empty", LibreSCRS::Plugin::FieldType::Text, {}});

    auto* section = LibreSCRS::FieldSectionBuilder::build("Test", group, {});
    auto edits = section->findChildren<QLineEdit*>();
    ASSERT_EQ(edits.size(), 1);
    EXPECT_EQ(edits[0]->text(), "A");

    delete section;
}

TEST_F(FieldSectionBuilderTest, UsesTranslationMap)
{
    LibreSCRS::Plugin::CardFieldGroup group;
    group.groupKey = "personal";
    group.fields.push_back({"given_name", "Given Name", LibreSCRS::Plugin::FieldType::Text, {'P'}});

    std::map<std::string, QString> translations = {{"given_name", "Ime"}};

    auto* section = LibreSCRS::FieldSectionBuilder::build("Personal", group, translations);
    auto labels = section->findChildren<QLabel*>();
    bool found = false;
    for (auto* label : labels) {
        if (label->text() == "Ime")
            found = true;
    }
    EXPECT_TRUE(found);

    delete section;
}

TEST_F(FieldSectionBuilderTest, FallsBackToKeyWhenNoTranslation)
{
    LibreSCRS::Plugin::CardFieldGroup group;
    group.groupKey = "test";
    group.fields.push_back({"my_field", "My Field", LibreSCRS::Plugin::FieldType::Text, {'X'}});

    auto* section = LibreSCRS::FieldSectionBuilder::build("Test", group, {});
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
    LibreSCRS::Plugin::CardFieldGroup group;
    group.groupKey = "personal";
    group.fields.push_back({"visible", "Visible", LibreSCRS::Plugin::FieldType::Text, {'A'}});
    group.fields.push_back({"hidden", "Hidden", LibreSCRS::Plugin::FieldType::Text, {'B'}});

    std::set<std::string> hidden = {"hidden"};
    auto* section = LibreSCRS::FieldSectionBuilder::build("Test", group, {}, hidden);
    auto edits = section->findChildren<QLineEdit*>();
    ASSERT_EQ(edits.size(), 1);
    EXPECT_EQ(edits[0]->text(), "A");

    delete section;
}
