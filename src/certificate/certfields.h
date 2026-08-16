// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
#pragma once

#include <LibreSCRS/AgentClient/Types.h>

#include <QLatin1StringView>
#include <QList>
#include <QPair>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>

/// @file
/// @brief Read access to the agent's grouped certificate field dictionary.
///
/// `CertificateInfo::extra["fields"]` is the agent's own grouping, forwarded
/// verbatim by the client library:
///
///     group key -> QVariantMap( field key -> QVariantList{ labelKey,
///                                                          labelFallback,
///                                                          value } )
///
/// Two translation units render it — the properties tree and the viewer's
/// General tab — so the walk, the cell decoding and the criticality
/// convention live here once. Everything is a read: nothing in this header
/// re-interprets, re-validates or re-orders what the agent sent, beyond
/// restoring the agent's ordinal ordering that the transport's map lost.
///
/// The group vocabulary is APPEND-ONLY on the wire. A group this build does
/// not name is ignored rather than guessed at — the client library documents
/// exactly that contract.
namespace librecelik::certfields {

/// The `CertificateInfo::extra` key the grouped fields land under.
inline constexpr QLatin1StringView kFieldsKey{"fields"};

/// Group emitted INSTEAD of every other one when the agent could not parse a
/// certificate's DER at all.
inline constexpr QLatin1StringView kDiagnosticGroup{"diagnostic"};

/// The marker a critical extension's label carries, for the extensions the
/// agent serves as a CELL: the `ext` dump's per-extension rows and the `cert`
/// group's two key identifiers.
inline constexpr QLatin1StringView kCriticalSuffix{" (Critical)"};

/// @brief Field key of the criticality flag a typed extension GROUP carries.
///
/// The other half of the same convention. An extension the agent serves as a
/// whole group has no label on the wire to suffix — `fields` is group key ->
/// field -> cell — so its criticality arrives as an ordinary cell,
/// `["", "Critical", "true"]`, present only when the extension is critical.
/// The empty label key is what marks it as GROUP METADATA: it describes the
/// group it sits in and is never rendered as a row of its own.
inline constexpr QLatin1StringView kCriticalField{"critical"};

/// @brief One decoded `[labelKey, labelFallback, value]` cell.
///
/// `labelFallback` has the criticality marker removed and `critical` carries
/// it instead, so a renderer can apply its own localized marker rather than
/// echoing the agent's English one.
struct Cell
{
    QString labelKey;
    QString labelFallback;
    QString value;
    bool critical = false;

    [[nodiscard]] bool isEmpty() const
    {
        return value.isEmpty();
    }
};

/// The whole field dictionary, or an empty map when the certificate carried
/// none (the key is ABSENT, not empty, in that case — see `CertificateInfo`).
[[nodiscard]] inline QVariantMap groupsOf(const LibreSCRS::AgentClient::CertificateInfo& cert)
{
    return cert.extra.value(QString(kFieldsKey)).toMap();
}

/// One group's cells, or an empty map when the certificate has no such group.
[[nodiscard]] inline QVariantMap groupOf(const QVariantMap& groups, QLatin1StringView group)
{
    return groups.value(QString(group)).toMap();
}

/// Decode one raw cell. A malformed or absent cell decodes to an empty Cell,
/// never to a partially-filled one.
[[nodiscard]] inline Cell toCell(const QVariant& raw)
{
    const QVariantList parts = raw.toList();
    if (parts.size() < 3)
        return {};
    Cell cell;
    cell.labelKey = parts.at(0).toString();
    cell.labelFallback = parts.at(1).toString();
    cell.value = parts.at(2).toString();
    if (cell.labelFallback.endsWith(kCriticalSuffix)) {
        cell.critical = true;
        cell.labelFallback.chop(kCriticalSuffix.size());
    }
    return cell;
}

/// One named cell out of one named group.
[[nodiscard]] inline Cell cellOf(const QVariantMap& groups, QLatin1StringView group, QLatin1StringView field)
{
    return toCell(groupOf(groups, group).value(QString(field)));
}

/// Shorthand for the value of one named cell.
[[nodiscard]] inline QString valueOf(const QVariantMap& groups, QLatin1StringView group, QLatin1StringView field)
{
    return cellOf(groups, group, field).value;
}

/// @brief True when the extension a typed group decodes was marked critical.
///
/// Reads the group's `critical` metadata cell (see @ref kCriticalField). False
/// for a group that carries no such cell — which is both "not critical" and
/// "this agent does not report criticality", two cases a renderer treats the
/// same way: no marker.
[[nodiscard]] inline bool groupIsCritical(const QVariantMap& groups, QLatin1StringView group)
{
    return toCell(groupOf(groups, group).value(QString(kCriticalField))).value == QLatin1StringView("true");
}

/// Trailing decimal ordinal of an ordinal-keyed field ("dns1" -> 1,
/// "policy10" -> 10), or -1 when the key carries none.
[[nodiscard]] inline int fieldOrdinal(const QString& fieldKey)
{
    qsizetype digits = 0;
    while (digits < fieldKey.size() && fieldKey.at(fieldKey.size() - 1 - digits).isDigit())
        ++digits;
    if (digits == 0)
        return -1;
    bool ok = false;
    const int ordinal = QStringView(fieldKey).right(digits).toInt(&ok);
    return ok ? ordinal : -1;
}

/// @brief A group's cells in the agent's own order.
///
/// The agent numbers repeated entries (`dns0`, `email1`, `url0`, `policy1`)
/// in certificate order, but both transports carry the group as a map, which
/// sorts its keys lexicographically — so `dns1` would precede `email0` and
/// `url10` would precede `url2`. Sorting on the ordinal first restores what
/// the certificate actually said; the key breaks ties for the non-ordinal
/// keys (`isCa`/`pathLen`) and for entries of different kinds sharing an
/// ordinal.
/// (The criticality flag is skipped: it describes the group, not one of its
/// entries, and a caller iterating a group's cells is building rows.)
[[nodiscard]] inline QList<QPair<QString, Cell>> orderedCells(const QVariantMap& group)
{
    QList<QPair<QString, Cell>> cells;
    cells.reserve(group.size());
    for (auto it = group.constBegin(); it != group.constEnd(); ++it) {
        if (it.key() == kCriticalField)
            continue;
        cells.append({it.key(), toCell(it.value())});
    }
    std::stable_sort(cells.begin(), cells.end(), [](const auto& lhs, const auto& rhs) {
        const int lo = fieldOrdinal(lhs.first);
        const int ro = fieldOrdinal(rhs.first);
        if (lo != ro)
            return lo < ro;
        return lhs.first < rhs.first;
    });
    return cells;
}

/// @brief True when the agent reported a certificate it could not parse.
///
/// Two shapes mean the same thing to a viewer: the agent said so explicitly
/// (the `diagnostic` group, which it emits instead of all the others), or the
/// record arrived with no field dictionary and no display DN at all.
[[nodiscard]] inline bool isUnparseable(const LibreSCRS::AgentClient::CertificateInfo& cert)
{
    const QVariantMap groups = groupsOf(cert);
    if (groups.contains(QString(kDiagnosticGroup)))
        return true;
    return groups.isEmpty() && cert.subject.isEmpty() && cert.issuer.isEmpty();
}

/// The agent's own parse-failure detail, or an empty string when it reported
/// none.
[[nodiscard]] inline QString parseErrorDetail(const LibreSCRS::AgentClient::CertificateInfo& cert)
{
    return valueOf(groupsOf(cert), kDiagnosticGroup, QLatin1StringView("parseError"));
}

} // namespace librecelik::certfields
