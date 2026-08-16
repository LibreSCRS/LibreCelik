// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// The photo merge is pure — a field model in, a field model out — but its one
// impure dependency is the sealed-descriptor reader, so every case here hands
// it a descriptor of exactly the shape the production producer creates rather
// than a stand-in the reader would refuse for a reason the merge never sees.

#include "agent/fieldmerge.h"

#include <LibreSCRS/AgentClient/FdHandle.h>
#include <LibreSCRS/AgentClient/SignOptions.h> // PhotoItem
#include <LibreSCRS/AgentClient/Types.h>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

TEST(FieldMerge, PhotoBecomesDetailFieldInPhotoGroup)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "memfd sealing is the Linux producer's shape; the merge "
                    "logic itself is platform-neutral";
#else
    using namespace LibreSCRS::AgentClient;
    // Fully-sealed memfd — the shape readBoundedPayload ACCEPTS on Linux
    // (a /tmp regular file may sit on tmpfs, which answers the seal query
    // seal-less and is refused — see SealedPayload.h's warning).
    const int fd = memfd_create("t", MFD_ALLOW_SEALING);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(write(fd, "abc", 3), 3);
    ASSERT_EQ(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW), 0);
    std::vector<PhotoItem> photos;
    // PRODUCTION-SHAPED key: the wire's "groupKey:fieldKey" composite
    // (GetPhotoOperation builds it; FakeSocketAgent serves exactly this).
    photos.push_back(PhotoItem{QStringLiteral("personal:photo"), FdHandle{fd}, {}});
    const auto merged = librecelik::agent::mergePhotoIntoGroups({}, std::move(photos));
    ASSERT_EQ(merged.size(), 1);
    EXPECT_EQ(merged[0].key, QStringLiteral("photo"));
    ASSERT_EQ(merged[0].fields.size(), 1);
    EXPECT_EQ(merged[0].fields[0].key, QStringLiteral("photo")); // split field part
    EXPECT_EQ(merged[0].fields[0].extra.value(QStringLiteral("wireKey")).toString(), QStringLiteral("personal:photo"));
    EXPECT_EQ(merged[0].fields[0].detail.toByteArray(), QByteArrayLiteral("abc"));
    // The composite's group half is preserved as provenance rather than
    // dropped: a multi-photo card's fields are only distinguishable by it.
    EXPECT_EQ(merged[0].fields[0].extra.value(QStringLiteral("sourceGroup")).toString(), QStringLiteral("personal"));
#endif
}

TEST(FieldMerge, ExistingPhotoGroupIsAppendedNotDuplicated)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "memfd sealing is the Linux producer's shape; the merge "
                    "logic itself is platform-neutral";
#else
    using namespace LibreSCRS::AgentClient;
    const int fd = memfd_create("t", MFD_ALLOW_SEALING);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(write(fd, "xy", 2), 2);
    ASSERT_EQ(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW), 0);

    // A card whose identity read ALREADY produced a "photo" group (the group
    // key is the wire's, not this merge's invention): the payload joins it
    // instead of creating a second group with the same key, which every
    // consumer keyed on the group would then see only half of.
    QList<FieldGroup> groups;
    groups.append(FieldGroup{QStringLiteral("personal"), {}, {}});
    groups.append(FieldGroup{QStringLiteral("photo"), {Field{QStringLiteral("signature"), {}, {}, {}}}, {}});

    std::vector<PhotoItem> photos;
    photos.push_back(PhotoItem{QStringLiteral("personal:photo"), FdHandle{fd}, {}});
    const auto merged = librecelik::agent::mergePhotoIntoGroups(groups, std::move(photos));

    ASSERT_EQ(merged.size(), 2); // no second "photo" group
    EXPECT_EQ(merged[0].key, QStringLiteral("personal"));
    EXPECT_EQ(merged[1].key, QStringLiteral("photo"));
    ASSERT_EQ(merged[1].fields.size(), 2);
    EXPECT_EQ(merged[1].fields[0].key, QStringLiteral("signature")); // pre-existing, kept first
    EXPECT_EQ(merged[1].fields[1].key, QStringLiteral("photo"));
    EXPECT_EQ(merged[1].fields[1].detail.toByteArray(), QByteArrayLiteral("xy"));
#endif
}

TEST(FieldMerge, ColonLessKeyKeptVerbatimAsFieldKey)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "memfd sealing is the Linux producer's shape; the merge "
                    "logic itself is platform-neutral";
#else
    using namespace LibreSCRS::AgentClient;
    const int fd = memfd_create("t", MFD_ALLOW_SEALING);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(write(fd, "z", 1), 1);
    ASSERT_EQ(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW), 0);

    // An agent that does NOT compose the key keeps its whole spelling as the
    // field key — never a split that would invent an empty group half.
    std::vector<PhotoItem> photos;
    photos.push_back(PhotoItem{QStringLiteral("portrait"), FdHandle{fd}, {}});
    const auto merged = librecelik::agent::mergePhotoIntoGroups({}, std::move(photos));

    ASSERT_EQ(merged.size(), 1);
    EXPECT_EQ(merged[0].key, QStringLiteral("photo"));
    ASSERT_EQ(merged[0].fields.size(), 1);
    EXPECT_EQ(merged[0].fields[0].key, QStringLiteral("portrait"));
    EXPECT_EQ(merged[0].fields[0].extra.value(QStringLiteral("wireKey")).toString(), QStringLiteral("portrait"));
    EXPECT_TRUE(merged[0].fields[0].extra.value(QStringLiteral("sourceGroup")).toString().isEmpty());
    EXPECT_EQ(merged[0].fields[0].detail.toByteArray(), QByteArrayLiteral("z"));
#endif
}
