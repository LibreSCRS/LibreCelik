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
///       extra = {"wireKey": item.key, "sourceGroup": sourceGroup}}
/// into a group with key "photo" (created if absent). A key with no ':'
/// keeps its full spelling as fieldKey. Unreadable/oversize payloads are
/// dropped silently (parity: a missing photo renders the placeholder).
/// Consumers select the portrait by fieldKey "photo" first, any field in
/// the "photo" group as fallback (multi-photo cards disambiguate by
/// fieldKey).
[[nodiscard]] QList<LibreSCRS::AgentClient::FieldGroup>
mergePhotoIntoGroups(QList<LibreSCRS::AgentClient::FieldGroup> groups,
                     std::vector<LibreSCRS::AgentClient::PhotoItem> photos);
} // namespace librecelik::agent
