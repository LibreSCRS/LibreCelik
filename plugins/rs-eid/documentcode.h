// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <plugin/fieldvalue.h>

#include <LibreSCRS/AgentClient/Types.h>

#include <QList>
#include <QString>

namespace librecelik::rseid {

/// The document code the card reports in meta/card_type: "ID", "IF", "RP" or
/// "EL". An absent or unknown code reads as "ID" (the citizen layout), so the
/// widget and the print formatter can never disagree about it.
[[nodiscard]] inline QString documentCode(const QList<LibreSCRS::AgentClient::FieldGroup>& groups)
{
    const QString code = librecelik::plugin::fieldValue(groups, u"card_type");
    if (code == u"IF" || code == u"RP" || code == u"EL")
        return code;
    return QStringLiteral("ID");
}

/// Translation id of the document's own name, shared by the on-screen header
/// and the print title: the citizen card is the plain "Serbian eID" header,
/// every other type names itself. Pass a code from documentCode().
[[nodiscard]] inline const char* documentNameId(const QString& code)
{
    if (code == u"IF")
        return "lc-eid-doc-title-foreigner";
    if (code == u"RP")
        return "lc-eid-doc-title-residence-permit";
    if (code == u"EL")
        return "lc-eid-doc-title-service-id";
    return "lc-eid-title";
}

} // namespace librecelik::rseid
