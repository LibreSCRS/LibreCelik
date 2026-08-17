// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once
#include <LibreSCRS/AgentClient/SignOptions.h> // PhotoItem
#include <LibreSCRS/AgentClient/Types.h>
#include <QList>
#include <vector>

namespace librecelik::agent {
/// Merge photo payloads into the field model (spec §5.4: widgets and print
/// templates consume the photo AS A FIELD).
///
/// WIRE KEY SHAPE (do not invent keys): PhotoItem::key is the agent's
/// "groupKey:fieldKey" composite — GetPhotoOperation builds
/// `g.groupKey + ":" + f.fieldKey` (e.g. "personal:photo" for rs-eid; the
/// client and FakeSocketAgent pass it through verbatim). Each item's key is
/// split on the FIRST ':' into (sourceGroup, fieldKey); the bytes are read
/// via readBoundedPayload and appended as
/// Field{key = fieldKey, value = {}, detail = QByteArray,
///       extra = {"wireKey": item.key, "sourceGroup": sourceGroup}}.
///
/// WHERE it lands: into the group named by sourceGroup, created if absent
/// and appended at the end (final section order is the widget's own staged
/// order, not this one). Every photo-typed field the card carries rides
/// this one channel — an eMRTD's DG7 handwritten signature arrives as
/// "signature:signature" beside its "photo:photo" portrait — so the group
/// half is what keeps them apart and renderable by their own sections.
///
/// The PORTRAIT is the exception: fieldKey "photo" always lands in the
/// "photo" group whatever group named it, because that is where every
/// consumer looks for it by name and the group half differs per card
/// (rs-eid "personal:photo", eMRTD "photo:photo"). A key with no ':' keeps
/// its full spelling as fieldKey and lands in "photo" too.
///
/// Unreadable/oversize payloads are dropped silently (parity: a missing
/// photo renders the placeholder). Consumers select the portrait by
/// fieldKey "photo" first, any field in the "photo" group as fallback
/// (multi-photo cards disambiguate by fieldKey).
[[nodiscard]] QList<LibreSCRS::AgentClient::FieldGroup>
mergePhotoIntoGroups(QList<LibreSCRS::AgentClient::FieldGroup> groups,
                     std::vector<LibreSCRS::AgentClient::PhotoItem> photos);
} // namespace librecelik::agent
