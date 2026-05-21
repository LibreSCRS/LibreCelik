// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "document/tokensection.h"

#include <LibreSCRS/Plugin/CardData.h>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>

#include <gtest/gtest.h>

using LibreSCRS::Plugin::CardField;
using LibreSCRS::Plugin::CardFieldGroup;
using LibreSCRS::Plugin::FieldType;

namespace {

class TokenSectionPlaceholderTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app = new QApplication(argc, argv);
        }
    }
    static QApplication* app;
};
QApplication* TokenSectionPlaceholderTest::app = nullptr;

// Regression guard: when middleware fails to read any token field (empty
// CardFieldGroup), the header card must still render so users see the
// structural UX even with no card data available. Pre-fix, an early
// `if (group.fields.empty()) return;` short-circuited rendering entirely.
TEST_F(TokenSectionPlaceholderTest, EmptyGroupStillRendersHeader)
{
    TokenSection section(nullptr);
    CardFieldGroup empty;

    section.setTokenInfo(empty);

    // Header card has 3 field-label QLabels (one per row) + 1 icon QLabel = 4.
    // Empty-group fallback path must produce a CardHeaderCard child.
    EXPECT_GT(section.findChildren<QLabel*>().size(), 0)
        << "Header card must render even when middleware returns empty group";
}

// Regression guard: when a known field is present but its value is empty,
// the rendered value cell must show the em-dash placeholder U+2014 rather
// than an empty / missing row. Constructs the group via direct member
// access because `addText` rejects empty values when fields is empty
// (CardData.h:127-138 throws std::logic_error).
TEST_F(TokenSectionPlaceholderTest, EmptyValueRendersEmDashPlaceholder)
{
    TokenSection section(nullptr);
    CardFieldGroup group;
    group.groupKey = "pki";

    // Direct member-access construction sidesteps addText's eager-validation
    // contract; the production plugin path uses addText only when it has
    // a non-empty value to insert.
    CardField labelField;
    labelField.key = "label";
    labelField.label = "Label";
    labelField.type = FieldType::Text;
    // value intentionally empty — exercises placeholder render
    group.fields.push_back(labelField);

    section.setTokenInfo(group);

    const auto values = section.findChildren<QLineEdit*>();
    const QString emDash = QString::fromUtf8("\xe2\x80\x94");
    bool hasPlaceholder = false;
    for (auto* v : values) {
        if (v->text() == emDash) {
            hasPlaceholder = true;
            break;
        }
    }
    EXPECT_TRUE(hasPlaceholder) << "Empty value must render em-dash (U+2014) placeholder";
}

} // namespace
