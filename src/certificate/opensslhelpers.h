// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QString>
#include <cstdint>
#include <memory>

// Forward declarations for OpenSSL types
typedef struct x509_st X509;
typedef struct x509_store_st X509_STORE;
typedef struct bio_st BIO;
typedef struct asn1_string_st ASN1_TIME;

namespace certutil {

struct X509Deleter
{
    void operator()(X509* p) const;
};

struct X509StoreDeleter
{
    void operator()(X509_STORE* p) const;
};

struct BioDeleter
{
    void operator()(BIO* p) const;
};

using X509Ptr = std::unique_ptr<X509, X509Deleter>;
using X509StorePtr = std::unique_ptr<X509_STORE, X509StoreDeleter>;
using BioPtr = std::unique_ptr<BIO, BioDeleter>;

/// Read BIO memory buffer and return as UTF-8 QString.
QString bioToQString(BIO* bio);

/// Read BIO memory buffer and return as Latin-1 QString (for ASN1_TIME).
QString bioToQLatin1(BIO* bio);

/// Format ASN1_TIME as a human-readable Latin-1 string.
QString asnTimeToString(const ASN1_TIME* time);

/// Parse DER-encoded certificate bytes into an X509Ptr.
X509Ptr parseDer(const uint8_t* data, size_t len);

} // namespace certutil
