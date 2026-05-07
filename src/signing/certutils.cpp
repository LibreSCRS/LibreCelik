// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certutils.h"

#include <LibreSCRS/Certificate/ParsedCertificate.h>

#include <QUrl>

#include <chrono>

namespace lcc = LibreSCRS::Certificate;

namespace signing {

QString subjectCN(const std::vector<uint8_t>& derBytes)
{
    return certNames(derBytes).subjectCN;
}

CertNames certNames(const std::vector<uint8_t>& derBytes)
{
    if (derBytes.empty())
        return {};
    auto cert = lcc::ParsedCertificate::fromDer(derBytes);
    if (!cert)
        return {};
    CertNames names;
    names.subjectCN = QString::fromStdString(cert->subject().commonName());
    names.issuerCN = QString::fromStdString(cert->issuer().commonName());
    return names;
}

bool isCertificateExpired(const std::vector<uint8_t>& derBytes)
{
    // Defensive: when we cannot determine validity (empty data or parse
    // failure), treat the certificate as expired so the caller's
    // expired-cert warning fires instead of silently letting signing proceed
    // with a potentially-bad cert.
    if (derBytes.empty())
        return true;
    auto cert = lcc::ParsedCertificate::fromDer(derBytes);
    if (!cert)
        return true;
    return cert->notAfter() <= std::chrono::system_clock::now();
}

bool isValidTsaUrl(const QString& url)
{
    if (url.isEmpty())
        return false;
    const QUrl parsed(url, QUrl::StrictMode);
    return parsed.isValid() && parsed.scheme() == QStringLiteral("https") && !parsed.host().isEmpty();
}

} // namespace signing
