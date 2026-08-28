// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// The photo merge is pure — a field model in, a field model out — but its one
// impure dependency is the sealed-descriptor reader, so every case here hands
// it a descriptor of exactly the shape the production producer creates rather
// than a stand-in the reader would refuse for a reason the merge never sees.

#include "agent/fieldmerge.h"

#include "qstring_printto.h"

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

#ifdef Q_OS_LINUX
namespace {

/// A payload descriptor of the shape readBoundedPayload accepts: a fully
/// sealed memfd, as the production producer creates.
int sealedFd(const char* bytes, int length)
{
    const int fd = memfd_create("t", MFD_ALLOW_SEALING);
    if (fd < 0 || write(fd, bytes, length) != length ||
        fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW) != 0) {
        return -1;
    }
    return fd;
}

/// The group of @p groups keyed @p key, or nullptr.
const LibreSCRS::AgentClient::FieldGroup* groupNamed(const QList<LibreSCRS::AgentClient::FieldGroup>& groups,
                                                     const QString& key)
{
    for (const LibreSCRS::AgentClient::FieldGroup& group : groups) {
        if (group.key == key) {
            return &group;
        }
    }
    return nullptr;
}

} // namespace
#endif

// A card may carry MORE than a portrait: the eMRTD family's DG7 is a
// handwritten signature, and the agent hands it over the same photo channel,
// keyed with its own group half. Every item landing in the portrait's group
// would leave a widget that renders a signature section nothing to render it
// from — the image would sit invisible among the portraits.
TEST(FieldMerge, SignatureLandsInTheGroupTheAgentNamed)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "memfd sealing is the Linux producer's shape; the merge "
                    "logic itself is platform-neutral";
#else
    using namespace LibreSCRS::AgentClient;
    const int portraitFd = sealedFd("face", 4);
    ASSERT_GE(portraitFd, 0);
    const int signatureFd = sealedFd("sig", 3);
    ASSERT_GE(signatureFd, 0);

    // The eMRTD shape: both photo-typed fields, each naming its own group.
    std::vector<PhotoItem> photos;
    photos.push_back(PhotoItem{QStringLiteral("photo:photo"), FdHandle{portraitFd}, {}});
    photos.push_back(PhotoItem{QStringLiteral("signature:signature"), FdHandle{signatureFd}, {}});
    const auto merged = librecelik::agent::mergePhotoIntoGroups({}, std::move(photos));

    ASSERT_EQ(merged.size(), 2);
    const FieldGroup* photoGroup = groupNamed(merged, QStringLiteral("photo"));
    ASSERT_NE(photoGroup, nullptr);
    ASSERT_EQ(photoGroup->fields.size(), 1);
    EXPECT_EQ(photoGroup->fields[0].key, QStringLiteral("photo"));
    EXPECT_EQ(photoGroup->fields[0].detail.toByteArray(), QByteArrayLiteral("face"));

    const FieldGroup* signatureGroup = groupNamed(merged, QStringLiteral("signature"));
    ASSERT_NE(signatureGroup, nullptr);
    ASSERT_EQ(signatureGroup->fields.size(), 1);
    EXPECT_EQ(signatureGroup->fields[0].key, QStringLiteral("signature"));
    EXPECT_EQ(signatureGroup->fields[0].detail.toByteArray(), QByteArrayLiteral("sig"));
    // Provenance is kept exactly as it is for a portrait.
    EXPECT_EQ(signatureGroup->fields[0].extra.value(QStringLiteral("wireKey")).toString(),
              QStringLiteral("signature:signature"));
    EXPECT_EQ(signatureGroup->fields[0].extra.value(QStringLiteral("sourceGroup")).toString(),
              QStringLiteral("signature"));
#endif
}

TEST(FieldMerge, NonPortraitJoinsAnExistingGroupRatherThanASecondOne)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "memfd sealing is the Linux producer's shape; the merge "
                    "logic itself is platform-neutral";
#else
    using namespace LibreSCRS::AgentClient;
    const int fd = sealedFd("sig", 3);
    ASSERT_GE(fd, 0);

    // The identity read already produced the group the item names; the payload
    // joins it, exactly as a portrait joins an existing "photo" group.
    QList<FieldGroup> groups;
    groups.append(FieldGroup{QStringLiteral("signature"), {Field{QStringLiteral("present"), {}, {}, {}}}, {}});

    std::vector<PhotoItem> photos;
    photos.push_back(PhotoItem{QStringLiteral("signature:signature"), FdHandle{fd}, {}});
    const auto merged = librecelik::agent::mergePhotoIntoGroups(groups, std::move(photos));

    ASSERT_EQ(merged.size(), 1); // no second "signature" group
    ASSERT_EQ(merged[0].fields.size(), 2);
    EXPECT_EQ(merged[0].fields[0].key, QStringLiteral("present")); // pre-existing, kept first
    EXPECT_EQ(merged[0].fields[1].key, QStringLiteral("signature"));
#endif
}

// The portrait's landing group is NOT the one the agent named it in — an
// rs-eid portrait arrives as "personal:photo" and every consumer of it (both
// widgets, both print templates, and the controller's photo-first streaming)
// looks it up in the "photo" group by name. Routing it to "personal" would
// take the portrait off all five of those paths.
TEST(FieldMerge, PortraitStaysInThePhotoGroupWhateverGroupNamedIt)
{
#ifndef Q_OS_LINUX
    GTEST_SKIP() << "memfd sealing is the Linux producer's shape; the merge "
                    "logic itself is platform-neutral";
#else
    using namespace LibreSCRS::AgentClient;
    const int fd = sealedFd("face", 4);
    ASSERT_GE(fd, 0);

    std::vector<PhotoItem> photos;
    photos.push_back(PhotoItem{QStringLiteral("personal:photo"), FdHandle{fd}, {}});
    const auto merged = librecelik::agent::mergePhotoIntoGroups({}, std::move(photos));

    ASSERT_EQ(merged.size(), 1);
    EXPECT_EQ(merged[0].key, QStringLiteral("photo"));
    EXPECT_EQ(groupNamed(merged, QStringLiteral("personal")), nullptr);
    ASSERT_EQ(merged[0].fields.size(), 1);
    EXPECT_EQ(merged[0].fields[0].detail.toByteArray(), QByteArrayLiteral("face"));
    // Provenance still records where it came from.
    EXPECT_EQ(merged[0].fields[0].extra.value(QStringLiteral("sourceGroup")).toString(), QStringLiteral("personal"));
#endif
}

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
