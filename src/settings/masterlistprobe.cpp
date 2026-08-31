// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "settings/masterlistprobe.h"

#include <QList>

#include <cstring>

namespace librecelik::settings {

namespace {

/// `id-signedData` (1.2.840.113549.1.7.2) as a complete DER OBJECT IDENTIFIER —
/// tag, length and value — so it can be compared as a byte run at the one place
/// a CMS ContentInfo puts it: immediately inside the outer SEQUENCE.
constexpr char kSignedDataOid[] = "\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x07\x02";
constexpr int kSignedDataOidSize = 11;

bool isAttributeTypeStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

/// Attribute descriptions are a type plus options: `pkdMasterListContent;binary`.
/// Numeric OIDs are legal types too, hence the dot.
bool isAttributeTypeChar(char c)
{
    return isAttributeTypeStart(c) || c == '-' || c == '.' || c == ';';
}

/// One complete CMS ContentInfo carrying id-signedData, or not.
///
/// The length is checked to cover the value EXACTLY, so a truncated or
/// concatenated blob is not counted — except under the BER indefinite form,
/// where the length is not declared up front and the end-of-contents octets are
/// what close the object.
bool isSignedObject(const QByteArray& value)
{
    constexpr int kMinimumSize = 2 + kSignedDataOidSize;
    if (value.size() < kMinimumSize)
        return false;
    if (static_cast<unsigned char>(value.at(0)) != 0x30U)
        return false; // not a constructed SEQUENCE, so not a ContentInfo

    const auto lengthByte = static_cast<unsigned char>(value.at(1));
    int header = 0;
    if (lengthByte == 0x80U) {
        // BER indefinite length: closed by two zero octets rather than declared.
        if (value.size() < 2 || static_cast<unsigned char>(value.at(value.size() - 1)) != 0x00U ||
            static_cast<unsigned char>(value.at(value.size() - 2)) != 0x00U) {
            return false;
        }
        header = 2;
    } else if (lengthByte < 0x80U) {
        if (2 + static_cast<int>(lengthByte) != value.size())
            return false;
        header = 2;
    } else {
        const int octets = static_cast<int>(lengthByte & 0x7FU);
        if (octets < 1 || octets > 4 || value.size() < 2 + octets)
            return false;
        qint64 length = 0;
        for (int i = 0; i < octets; ++i)
            length = (length << 8) | static_cast<unsigned char>(value.at(2 + i));
        if (2 + octets + length != value.size())
            return false;
        header = 2 + octets;
    }

    if (value.size() < header + kSignedDataOidSize)
        return false;
    return std::memcmp(value.constData() + header, kSignedDataOid, kSignedDataOidSize) == 0;
}

/// The logical lines of @p bytes, with RFC 2849 folding undone: a line that
/// begins with a single space continues the one before it.
QList<QByteArray> unfold(const QByteArray& bytes)
{
    QList<QByteArray> logical;
    QByteArray current;
    bool haveCurrent = false;
    qsizetype at = 0;
    while (at <= bytes.size()) {
        qsizetype end = bytes.indexOf('\n', at);
        if (end < 0)
            end = bytes.size();
        QByteArray line = bytes.mid(at, end - at);
        if (line.endsWith('\r'))
            line.chop(1);

        if (line.startsWith(' ')) {
            // A continuation, even of nothing: a file that opens with one is
            // malformed, and the caller's grammar check will say so.
            current += line.mid(1);
            haveCurrent = true;
        } else {
            if (haveCurrent)
                logical.append(current);
            current = line;
            haveCurrent = true;
        }
        if (end == bytes.size())
            break;
        at = end + 1;
    }
    if (haveCurrent)
        logical.append(current);
    return logical;
}

} // namespace

MasterListFileProbe probeMasterListFile(const QByteArray& bytes)
{
    MasterListFileProbe probe;
    if (bytes.isEmpty())
        return probe; // nothing to recognise; the agent's own refusal is the honest one

    // LDIF is TEXT. A master list is a binary CMS object and is full of zero
    // octets, so this settles the common case before a line is looked at — and
    // it stops a stray 0x0A inside DER from being read as a line break.
    if (bytes.contains('\0'))
        return probe;

    bool sawDistinguishedName = false;
    int signedObjects = 0;

    for (const QByteArray& line : unfold(bytes)) {
        if (line.isEmpty())
            continue; // the record separator
        if (line.startsWith('#'))
            continue; // a comment

        const qsizetype colon = line.indexOf(':');
        if (colon < 1)
            return {}; // no attribute description: not LDIF
        if (!isAttributeTypeStart(line.at(0)))
            return {};
        for (qsizetype i = 1; i < colon; ++i) {
            if (!isAttributeTypeChar(line.at(i)))
                return {};
        }

        const QByteArray type = line.left(colon).split(';').constFirst().toLower();
        if (type == "dn")
            sawDistinguishedName = true;

        // `::` is a base64 value, `:<` names a URL to fetch one from, and a bare
        // `:` is the value itself. Only the first can carry a signed object: a
        // URL is not content, and this application does not follow one.
        if (colon + 1 < line.size() && line.at(colon + 1) == ':') {
            const QByteArray encoded = line.mid(colon + 2).trimmed();
            const QByteArray decoded =
                QByteArray::fromBase64(encoded, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
            if (isSignedObject(decoded))
                ++signedObjects;
        }
    }

    if (!sawDistinguishedName)
        return {}; // every LDIF record names a dn; without one this is some other text

    probe.kind = signedObjects > 0 ? MasterListFileKind::LdifCollection : MasterListFileKind::LdifWithoutLists;
    probe.signedObjects = signedObjects;
    return probe;
}

} // namespace librecelik::settings
