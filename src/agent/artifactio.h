// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

// src/agent/artifactio.h — the ONE consumer of a signed-artifact fd,
// header-only so production (LiveSignController::writeArtifact) and the CI
// test compile the SAME code — the optionalsections.h pattern.

#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <cerrno>
#include <sys/types.h>
#include <unistd.h>

namespace librecelik::agent {

/// @brief Stream a signed-artifact fd into @p path atomically. Returns the
///        path on success, an empty string on any failure.
///
/// The wire hands the artifact memfd with its offset wherever the producer
/// left it — at EOF, after writing — so the stream REWINDS first (tolerating
/// ESPIPE for pipe-like fds, the same posture as the e2e sign-proof reader).
/// An artifact that yields ZERO bytes is a failure, never a success: no
/// signed container is empty, and committing one would report success over
/// a file with no signature in it (the Leg-1 bench catch, 2026-08-17).
///
/// Fixed-size streaming, NOT a bounded whole-payload read: a signed
/// artifact — an ASiC-E container above all — is unbounded by design.
[[nodiscard]] inline QString writeArtifactTo(int fd, const QString& path)
{
    if (path.isEmpty() || fd < 0) {
        return {};
    }
    if (::lseek(fd, 0, SEEK_SET) == static_cast<off_t>(-1) && errno != ESPIPE) {
        return {};
    }

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        return {};
    }
    constexpr qint64 kChunkBytes = 64 * 1024;
    QByteArray chunk(kChunkBytes, '\0');
    qint64 total = 0;
    for (;;) {
        const ssize_t taken = ::read(fd, chunk.data(), static_cast<std::size_t>(kChunkBytes));
        if (taken < 0) {
            if (errno == EINTR) {
                continue; // a signal is not a failure
            }
            return {}; // QSaveFile discards the partial file on destruction
        }
        if (taken == 0) {
            break;
        }
        if (out.write(chunk.constData(), taken) != taken) {
            return {};
        }
        total += taken;
    }
    if (total == 0) {
        return {};
    }
    return out.commit() ? path : QString();
}

} // namespace librecelik::agent
