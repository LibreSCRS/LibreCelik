// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors
//
// Ports every case of the retired middleware card-data helper suite onto the
// agent's field-group types. The lookups the GUI needs are unchanged in
// MEANING — group-scoped, group-key scoped, and flat first-match — only the
// carrier changed, so the case list stays recognisably the same one and any
// semantic drift shows up as a failing port rather than as a quietly dropped
// case.

#include "plugin/fieldvalue.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <gtest/gtest.h>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariant>

namespace {

using librecelik::plugin::fieldDetailBytes;
using librecelik::plugin::fieldValue;
using librecelik::plugin::findGroup;
using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

/// The retired suite's fixture, field for field: a "personal" group with two
/// text fields and a "document" group with one.
QList<FieldGroup> makeFixture()
{
    FieldGroup personal;
    personal.key = QStringLiteral("personal");
    personal.fields.append(Field{QStringLiteral("surname"), QStringLiteral("Petrović"), {}, {}});
    personal.fields.append(Field{QStringLiteral("given_name"), QStringLiteral("Ana"), {}, {}});

    FieldGroup document;
    document.key = QStringLiteral("document");
    document.fields.append(Field{QStringLiteral("document_number"), QStringLiteral("123456789"), {}, {}});

    return QList<FieldGroup>{personal, document};
}

} // namespace

TEST(FieldValue, StagedForBuildOrdersGroupsByTheWidgetsOwnStageList)
{
    // The Leg-6 bench catch generalized: the final wire model's group order
    // is delivery-dependent (a recovered read hands it over keyed, not
    // staged), while every streaming widget lays sections out in ARRIVAL
    // order — the vehicle page rendered its car-icon header mid-page. The
    // full-model ctors stage the model into the widget's own branch order;
    // unknown keys keep their relative order AFTER every staged one (data
    // preserved, never reordered ahead of designed sections).
    using librecelik::plugin::stagedForBuild;
    QList<LibreSCRS::AgentClient::FieldGroup> model;
    for (const char* key : {"owner", "national", "vehicle", "registration", "mystery_a", "mystery_b"}) {
        LibreSCRS::AgentClient::FieldGroup g;
        g.key = QString::fromLatin1(key);
        model.append(g);
    }
    const QList<LibreSCRS::AgentClient::FieldGroup> staged =
        stagedForBuild(model, {u"registration", u"vehicle", u"holder", u"owner", u"user", u"national"});
    QStringList keys;
    for (const auto& g : staged)
        keys << g.key;
    EXPECT_EQ(keys,
              (QStringList{QStringLiteral("registration"), QStringLiteral("vehicle"), QStringLiteral("owner"),
                           QStringLiteral("national"), QStringLiteral("mystery_a"), QStringLiteral("mystery_b")}));
}

TEST(FieldValue, GroupOverloadFindsPresentField)
{
    const auto groups = makeFixture();
    const FieldGroup* group = findGroup(groups, u"personal");
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(fieldValue(*group, u"surname"), QStringLiteral("Petrović"));
}

TEST(FieldValue, FindGroupReturnsNullForAbsentGroup)
{
    const auto groups = makeFixture();
    EXPECT_EQ(findGroup(groups, u"nonexistent_group"), nullptr);
}

TEST(FieldValue, GroupOverloadReturnsEmptyForMissingField)
{
    const auto groups = makeFixture();
    const FieldGroup* group = findGroup(groups, u"personal");
    ASSERT_NE(group, nullptr);
    EXPECT_TRUE(fieldValue(*group, u"nonexistent").isEmpty());
}

TEST(FieldValue, GroupKeyOverloadFindsPresentField)
{
    const auto groups = makeFixture();
    EXPECT_EQ(fieldValue(groups, u"personal", u"given_name"), QStringLiteral("Ana"));
    EXPECT_EQ(fieldValue(groups, u"document", u"document_number"), QStringLiteral("123456789"));
}

TEST(FieldValue, GroupKeyOverloadReturnsEmptyForMissingGroup)
{
    const auto groups = makeFixture();
    EXPECT_TRUE(fieldValue(groups, u"nonexistent_group", u"anything").isEmpty());
}

TEST(FieldValue, GroupKeyOverloadReturnsEmptyForMissingFieldInPresentGroup)
{
    const auto groups = makeFixture();
    EXPECT_TRUE(fieldValue(groups, u"personal", u"middle_name").isEmpty());
}

TEST(FieldValue, FlatLookupFindsFirstMatchAcrossGroups)
{
    using namespace LibreSCRS::AgentClient;
    FieldGroup a{QStringLiteral("personal"), {Field{QStringLiteral("given_name"), QStringLiteral("Ana"), {}, {}}}, {}};
    FieldGroup b{QStringLiteral("address"), {Field{QStringLiteral("city"), QStringLiteral("Niš"), {}, {}}}, {}};
    EXPECT_EQ(librecelik::plugin::fieldValue({a, b}, u"city"), QStringLiteral("Niš"));
    EXPECT_TRUE(librecelik::plugin::fieldValue({a, b}, u"absent").isEmpty());
}

TEST(FieldValue, FlatLookupPrefersTheEarlierGroupOnADuplicateKey)
{
    FieldGroup first;
    first.key = QStringLiteral("personal");
    first.fields.append(Field{QStringLiteral("city"), QStringLiteral("Niš"), {}, {}});

    FieldGroup second;
    second.key = QStringLiteral("address");
    second.fields.append(Field{QStringLiteral("city"), QStringLiteral("Novi Sad"), {}, {}});

    EXPECT_EQ(fieldValue(QList<FieldGroup>{first, second}, u"city"), QStringLiteral("Niš"));
}

TEST(FieldValue, FlatLookupReturnsEmptyForMissingField)
{
    const auto groups = makeFixture();
    EXPECT_TRUE(fieldValue(groups, u"nope").isEmpty());
}

// The merged photo is the reason `Field::detail` exists on this boundary: its
// bytes never travel through `value`, so a text lookup must miss it and the
// byte lookup must find it.
TEST(FieldValue, DetailBytesReadTheBinaryPayloadTheTextLookupCannotSee)
{
    FieldGroup photo;
    photo.key = QStringLiteral("photo");
    Field portrait;
    portrait.key = QStringLiteral("photo");
    portrait.detail = QVariant::fromValue(QByteArrayLiteral("\x89PNG"));
    photo.fields.append(portrait);

    const QList<FieldGroup> groups{photo};
    EXPECT_EQ(fieldDetailBytes(groups, u"photo", u"photo"), QByteArrayLiteral("\x89PNG"));
    EXPECT_TRUE(fieldValue(groups, u"photo", u"photo").isEmpty());
}

TEST(FieldValue, DetailBytesReturnEmptyForMissingGroupOrField)
{
    const auto groups = makeFixture();
    EXPECT_TRUE(fieldDetailBytes(groups, u"photo", u"photo").isEmpty());
    EXPECT_TRUE(fieldDetailBytes(groups, u"personal", u"photo").isEmpty());
}
