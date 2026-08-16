// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// Pure over its inputs except for the one read it cannot avoid: the payload
// bytes live behind a descriptor. That read goes through the client's single
// audited reader, never a hand-rolled stat+read here, so the byte cap and the
// descriptor-shape rules apply to LC exactly as they do to every other
// consumer of the agent's sealed payloads.

#include "agent/fieldmerge.h"

#include <LibreSCRS/AgentClient/SealedPayload.h> // readBoundedPayload

#include <QByteArray>
#include <QLatin1Char>
#include <QString>
#include <QVariant>

#include <optional>

namespace librecelik::agent {

using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;
using LibreSCRS::AgentClient::PhotoItem;

namespace {

/// The group key the merged photo fields live under. The wire's own spelling —
/// an identity read may already have produced a group with this key, and the
/// merge joins that one rather than shadowing it with a second.
const QString& photoGroupKey()
{
    static const QString key = QStringLiteral("photo");
    return key;
}

/// The photo group of @p groups, appended first when the model has none.
FieldGroup& photoGroupIn(QList<FieldGroup>& groups)
{
    for (FieldGroup& group : groups) {
        if (group.key == photoGroupKey()) {
            return group;
        }
    }
    groups.append(FieldGroup{photoGroupKey(), {}, {}});
    return groups.last();
}

} // namespace

QList<FieldGroup> mergePhotoIntoGroups(QList<FieldGroup> groups, std::vector<PhotoItem> photos)
{
    for (PhotoItem& item : photos) {
        const std::optional<QByteArray> bytes = LibreSCRS::AgentClient::readBoundedPayload(item.fd);
        if (!bytes.has_value()) {
            // Silently dropped, by design: a refused or unreadable payload is
            // indistinguishable to the user from a card that carries no
            // portrait, and both render the placeholder. Surfacing it as an
            // error would fail an otherwise complete identity read.
            continue;
        }

        // Split on the FIRST ':' only — a field key may legitimately contain
        // one, and only the leading segment is the group half.
        const qsizetype separator = item.key.indexOf(QLatin1Char(':'));
        const QString sourceGroup = separator < 0 ? QString() : item.key.left(separator);
        const QString fieldKey = separator < 0 ? item.key : item.key.mid(separator + 1);

        Field field;
        field.key = fieldKey;
        field.detail = QVariant::fromValue(*bytes);
        // The composite key is kept verbatim as provenance: it is what the
        // agent said, and a consumer round-tripping a field back to the wire
        // must not have to re-assemble it from the split halves.
        field.extra.insert(QStringLiteral("wireKey"), item.key);
        field.extra.insert(QStringLiteral("sourceGroup"), sourceGroup);

        photoGroupIn(groups).fields.append(field);
    }
    return groups;
}

} // namespace librecelik::agent
