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

// isValidTsaUrl moved to signing/tsavalidation.h — it never touched a
// certificate, and this header's remaining entry points all parse DER
// in-process.

} // namespace signing
