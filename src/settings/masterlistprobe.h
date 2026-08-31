// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QByteArray>

namespace librecelik::settings {

/// @brief What a file a reader chose for the master-list import turned out to
///        be, decided by its SHAPE.
///
/// The extension is a hint and never a contract: this answers what the bytes
/// are. Only `NotLdif` is offered to the agent — everything else is a file the
/// agent could only refuse, and refusing it here saves the reader an
/// authorization ceremony spent on a foregone answer.
enum class MasterListFileKind {
    /// Not an LDIF. Hand it to the agent exactly as before; the agent decides
    /// whether it is a master list, and this probe deliberately does not.
    NotLdif,
    /// An LDIF carrying signed objects — the form the ICAO Public Key Directory
    /// serves its master-list collection in. A COLLECTION is not a list: it
    /// holds many, each signed by its own publisher.
    LdifCollection,
    /// An LDIF that carries no signed object at all, so there is nothing in it
    /// that could be a master list.
    LdifWithoutLists,
};

/// @brief The verdict, plus the count that makes the collection case sayable.
struct MasterListFileProbe
{
    MasterListFileKind kind = MasterListFileKind::NotLdif;
    /// How many signed objects the LDIF carried. Zero unless @c kind is
    /// `LdifCollection`.
    int signedObjects = 0;
};

/// @brief Decide what @p bytes are.
///
/// LDIF is RFC 2849: records separated by blank lines, `attribute: value`,
/// `attribute:: <base64>` for a binary value, `attribute:< <url>` for a
/// referenced one, `#` comments, and continuation lines beginning with a single
/// space. A file is treated as LDIF only when EVERY logical line fits that
/// grammar and at least one of them is a `dn`, which is what keeps a text file
/// that happens to contain a colon from being read as a directory export.
///
/// A signed object is counted STRUCTURALLY, never by attribute name: a base64
/// value whose bytes are one CMS ContentInfo carrying id-signedData. The name
/// the publisher gives the attribute can change; what it holds cannot. Both the
/// DER definite-length and the BER indefinite-length encodings count, because
/// a real collection carries both.
[[nodiscard]] MasterListFileProbe probeMasterListFile(const QByteArray& bytes);

} // namespace librecelik::settings
