// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief writeArtifactTo — the one consumer of a signed-artifact fd.
///
/// The wire hands the artifact memfd with its offset at EOF (the producer
/// just wrote it), and the Leg-1 bench run proved what the old streaming
/// loop did with that: read from the current position, hit EOF instantly,
/// commit a ZERO-BYTE file, and report the row as signed. These cases pin
/// the two constraints: the stream rewinds before reading, and an empty
/// artifact is a failure, never a committed file.

#include "agent/artifactio.h"

#include "qstring_printto.h"

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include "anonymous_fd.h"

#include <gtest/gtest.h>

#include <sys/mman.h>
#include <unistd.h>

namespace {

/// A memfd carrying @p bytes with the offset left at EOF — the exact shape
/// the agent hands over.
int memfdAtEof(const QByteArray& bytes)
{
    const int fd = librecelik::test::makeAnonymousFd("artifactio-test");
    EXPECT_GE(fd, 0);
    if (fd >= 0 && !bytes.isEmpty()) {
        EXPECT_EQ(::write(fd, bytes.constData(), static_cast<std::size_t>(bytes.size())),
                  static_cast<ssize_t>(bytes.size()));
    }
    return fd;
}

} // namespace

TEST(ArtifactIo, RewindsTheEofOffsetAndWritesEveryByte)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QByteArray payload = QByteArrayLiteral("%PDF-1.5 signed artifact bytes");
    const int fd = memfdAtEof(payload);
    ASSERT_GE(fd, 0);

    const QString target = dir.filePath(QStringLiteral("signed.pdf"));
    EXPECT_EQ(librecelik::agent::writeArtifactTo(fd, target), target);

    QFile written(target);
    ASSERT_TRUE(written.open(QIODevice::ReadOnly));
    EXPECT_EQ(written.readAll(), payload);
    ::close(fd);
}

TEST(ArtifactIo, EmptyArtifactIsAFailureAndCommitsNoFile)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const int fd = memfdAtEof({});
    ASSERT_GE(fd, 0);

    const QString target = dir.filePath(QStringLiteral("signed.pdf"));
    EXPECT_TRUE(librecelik::agent::writeArtifactTo(fd, target).isEmpty());
    EXPECT_FALSE(QFile::exists(target)) << "a zero-byte artifact must never be committed as a signed file";
    ::close(fd);
}

TEST(ArtifactIo, InvalidFdAndEmptyPathAreFailures)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    EXPECT_TRUE(librecelik::agent::writeArtifactTo(-1, dir.filePath(QStringLiteral("x"))).isEmpty());
    const int fd = memfdAtEof(QByteArrayLiteral("x"));
    EXPECT_TRUE(librecelik::agent::writeArtifactTo(fd, QString()).isEmpty());
    ::close(fd);
}
