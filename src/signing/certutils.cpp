// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certutils.h"
#include "certificate/opensslhelpers.h"

#include <QUrl>
#include <openssl/x509.h>

namespace signing {

static QString extractCN(X509_NAME* name)
{
    if (!name)
        return {};
    char buf[256] = {};
    int len = X509_NAME_get_text_by_NID(name, NID_commonName, buf, sizeof(buf));
    if (len <= 0)
        return {};
    return QString::fromUtf8(buf, len);
}

QString subjectCN(const std::vector<uint8_t>& derBytes)
{
    return certNames(derBytes).subjectCN;
}

CertNames certNames(const std::vector<uint8_t>& derBytes)
{
    if (derBytes.empty())
        return {};
    auto x509 = certutil::parseDer(derBytes.data(), derBytes.size());
    if (!x509)
        return {};
    CertNames names;
    names.subjectCN = extractCN(X509_get_subject_name(x509.get()));
    names.issuerCN = extractCN(X509_get_issuer_name(x509.get()));
    return names;
}

bool isCertificateExpired(const std::vector<uint8_t>& derBytes)
{
    // Defensive: when we cannot determine validity (empty data, parse failure,
    // or malformed notAfter), treat the certificate as expired so the caller's
    // expired-cert warning fires instead of silently letting signing proceed
    // with a potentially-bad cert.
    if (derBytes.empty())
        return true;
    auto x509 = certutil::parseDer(derBytes.data(), derBytes.size());
    if (!x509)
        return true;
    // X509_cmp_current_time returns -1 if notAfter is before now, +1 if after,
    // and 0 on error or equal. Treat 0 conservatively as expired since a
    // malformed ASN1_TIME is indistinguishable from "exactly now".
    int cmp = X509_cmp_current_time(X509_get0_notAfter(x509.get()));
    return cmp <= 0;
}

bool isValidTsaUrl(const QString& url)
{
    if (url.isEmpty())
        return false;
    const QUrl parsed(url, QUrl::StrictMode);
    return parsed.isValid() && parsed.scheme() == QStringLiteral("https") && !parsed.host().isEmpty();
}

} // namespace signing
