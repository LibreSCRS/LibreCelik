// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "opensslhelpers.h"

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include <climits>

namespace certutil {

void X509Deleter::operator()(X509* p) const
{
    X509_free(p);
}

void X509StoreDeleter::operator()(X509_STORE* p) const
{
    X509_STORE_free(p);
}

void BioDeleter::operator()(BIO* p) const
{
    BIO_free(p);
}

QString bioToQString(BIO* bio)
{
    if (!bio)
        return {};
    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(bio, &bptr);
    if (!bptr || bptr->length == 0)
        return {};
    return QString::fromUtf8(bptr->data, static_cast<int>(bptr->length));
}

QString bioToQLatin1(BIO* bio)
{
    if (!bio)
        return {};
    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(bio, &bptr);
    if (!bptr || bptr->length == 0)
        return {};
    return QString::fromLatin1(bptr->data, static_cast<int>(bptr->length));
}

QString asnTimeToString(const ASN1_TIME* time)
{
    if (!time)
        return {};
    BioPtr bio(BIO_new(BIO_s_mem()));
    if (!bio)
        return {};
    ASN1_TIME_print(bio.get(), time);
    return bioToQLatin1(bio.get());
}

X509Ptr parseDer(const uint8_t* data, size_t len)
{
    if (!data || len == 0 || len > static_cast<size_t>(LONG_MAX))
        return {};
    const uint8_t* p = data;
    return X509Ptr(d2i_X509(nullptr, &p, static_cast<long>(len)));
}

} // namespace certutil
