// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#include "plugin/carddatautils.h"

#include <LibreSCRS/Plugin/CardData.h>
#include <LibreSCRS/Plugin/CardDataAccess.h>

#include <gtest/gtest.h>

#include <QString>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace {

using librecelik::plugin::getFieldValue;
using LibreSCRS::Plugin::CardData;
using LibreSCRS::Plugin::CardField;
using LibreSCRS::Plugin::CardFieldGroup;
using LibreSCRS::Plugin::FieldType;

CardData makeFixture()
{
    CardField surname;
    surname.key = "surname";
    surname.type = FieldType::Text;
    {
        std::string_view sv{"Petrović"};
        surname.value = std::vector<std::uint8_t>{sv.begin(), sv.end()};
    }

    CardField givenName;
    givenName.key = "given_name";
    givenName.type = FieldType::Text;
    {
        std::string_view sv{"Ana"};
        givenName.value = std::vector<std::uint8_t>{sv.begin(), sv.end()};
    }

    CardFieldGroup personal;
    personal.groupKey = "personal";
    personal.fields.push_back(surname);
    personal.fields.push_back(givenName);

    CardField docNumber;
    docNumber.key = "document_number";
    docNumber.type = FieldType::Text;
    {
        std::string_view sv{"123456789"};
        docNumber.value = std::vector<std::uint8_t>{sv.begin(), sv.end()};
    }

    CardFieldGroup document;
    document.groupKey = "document";
    document.fields.push_back(docNumber);

    CardData data;
    data.groups.push_back(personal);
    data.groups.push_back(document);
    return data;
}

} // namespace

TEST(CardDataUtilsTest, GroupPointerOverload_PresentField)
{
    auto data = makeFixture();
    const auto* group = &data.groupAt(*data.findGroup("personal"));
    EXPECT_EQ(getFieldValue(group, "surname"), QString("Petrović"));
}

TEST(CardDataUtilsTest, GroupPointerOverload_NullGroup)
{
    EXPECT_EQ(getFieldValue(static_cast<const CardFieldGroup*>(nullptr), "anything"), QString{});
}

TEST(CardDataUtilsTest, GroupPointerOverload_MissingField)
{
    auto data = makeFixture();
    const auto* group = &data.groupAt(*data.findGroup("personal"));
    EXPECT_EQ(getFieldValue(group, "nonexistent"), QString{});
}

TEST(CardDataUtilsTest, CardDataGroupKeyOverload_PresentField)
{
    auto data = makeFixture();
    EXPECT_EQ(getFieldValue(data, std::string{"personal"}, std::string{"given_name"}), QString("Ana"));
    EXPECT_EQ(getFieldValue(data, std::string{"document"}, std::string{"document_number"}), QString("123456789"));
}

TEST(CardDataUtilsTest, CardDataGroupKeyOverload_MissingGroup)
{
    auto data = makeFixture();
    EXPECT_EQ(getFieldValue(data, std::string{"nonexistent_group"}, std::string{"anything"}), QString{});
}

TEST(CardDataUtilsTest, CardDataGroupKeyOverload_MissingFieldInPresentGroup)
{
    auto data = makeFixture();
    EXPECT_EQ(getFieldValue(data, std::string{"personal"}, std::string{"middle_name"}), QString{});
}

TEST(CardDataUtilsTest, OptionalIndexOverload_EngagedIndex)
{
    auto data = makeFixture();
    auto idx = data.findGroup("personal");
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(getFieldValue(data, idx, std::string{"surname"}), QString("Petrović"));
}

TEST(CardDataUtilsTest, OptionalIndexOverload_DisengagedIndex)
{
    auto data = makeFixture();
    EXPECT_EQ(getFieldValue(data, std::optional<std::size_t>{}, std::string{"surname"}), QString{});
}

TEST(CardDataUtilsTest, FlatFieldKeyOverload_FindsFirstMatch)
{
    auto data = makeFixture();
    EXPECT_EQ(getFieldValue(data, std::string{"surname"}), QString("Petrović"));
    EXPECT_EQ(getFieldValue(data, std::string{"document_number"}), QString("123456789"));
}

TEST(CardDataUtilsTest, FlatFieldKeyOverload_MissingField)
{
    auto data = makeFixture();
    EXPECT_EQ(getFieldValue(data, std::string{"nope"}), QString{});
}

// Parity with the raw LM Wave 6 accessors — guards against future
// LC-side reimplementations drifting from LibreSCRS::Plugin::textValue
// semantics (e.g. first-match handling when multiple groups share a key).
TEST(CardDataUtilsTest, ParityWithLibreSCRSPluginTextValue)
{
    auto data = makeFixture();
    auto direct = LibreSCRS::Plugin::textValue(data, std::string_view{"personal"}, std::string_view{"surname"});
    ASSERT_TRUE(direct.has_value());
    EXPECT_EQ(getFieldValue(data, std::string{"personal"}, std::string{"surname"}), QString::fromStdString(*direct));
}

TEST(CardDataUtilsTest, ParityWithLibreSCRSPluginTextValueAt)
{
    auto data = makeFixture();
    auto idx = data.findGroup("document");
    ASSERT_TRUE(idx.has_value());
    auto direct = LibreSCRS::Plugin::textValueAt(data, *idx, std::string_view{"document_number"});
    ASSERT_TRUE(direct.has_value());
    EXPECT_EQ(getFieldValue(data, idx, std::string{"document_number"}), QString::fromStdString(*direct));
}
