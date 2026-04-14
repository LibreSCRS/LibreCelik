// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QString>
#include <cstdint>
#include <vector>

namespace signing {

// Extract subject CN from DER-encoded X.509 certificate.
// Returns empty string on failure.
QString subjectCN(const std::vector<uint8_t>& derBytes);

// Extract both subject and issuer CN in a single parse.
struct CertNames
{
    QString subjectCN;
    QString issuerCN;
};
CertNames certNames(const std::vector<uint8_t>& derBytes);

// Returns true if the certificate's notAfter date is in the past.
bool isCertificateExpired(const std::vector<uint8_t>& derBytes);

// Returns true if `url` is an acceptable TSA endpoint: well-formed under
// QUrl::StrictMode, scheme is https, host is non-empty. Rejects http://
// (no transport integrity on the token exchange), malformed URLs
// ("https:/typo.com"), and empty input. Used by FileSelectionPage and
// SignPage as defensive validation before signing.
bool isValidTsaUrl(const QString& url);

} // namespace signing
