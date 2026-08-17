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

/// The group key the PORTRAIT lives under, whatever group the agent named it
/// in. The wire's own spelling — an identity read may already have produced a
/// group with this key, and the merge joins that one rather than shadowing it
/// with a second.
const QString& photoGroupKey()
{
    static const QString key = QStringLiteral("photo");
    return key;
}

/// The field key that makes a payload the portrait rather than some other
/// image the card happens to carry.
const QString& portraitFieldKey()
{
    static const QString key = QStringLiteral("photo");
    return key;
}

/// The group of @p groups keyed @p key, appended when the model has none.
FieldGroup& groupIn(QList<FieldGroup>& groups, const QString& key)
{
    for (FieldGroup& group : groups) {
        if (group.key == key) {
            return group;
        }
    }
    groups.append(FieldGroup{key, {}, {}});
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

        // WHERE the payload lands. Every photo-typed field a card carries
        // arrives on this ONE channel — an eMRTD's DG7 handwritten signature
        // beside its DG2 portrait — and only the group half of the key tells
        // them apart. Dropping them all into the portrait's group leaves a
        // widget that renders a signature section nothing to render it from,
        // and the image sits invisible among the portraits.
        //
        // The PORTRAIT is the deliberate exception: it lands under "photo"
        // whatever group named it, because "photo" is where every consumer
        // looks for it BY NAME — both card widgets, both print templates, and
        // the controller's photo-first streaming — and the group half is not
        // the same on every card (rs-eid sends "personal:photo", eMRTD
        // "photo:photo"). The provenance above still records where it came
        // from, so nothing is lost by landing it in one place.
        const QString targetGroup =
            (fieldKey == portraitFieldKey() || sourceGroup.isEmpty()) ? photoGroupKey() : sourceGroup;
        groupIn(groups, targetGroup).fields.append(field);
    }
    return groups;
}

} // namespace librecelik::agent
