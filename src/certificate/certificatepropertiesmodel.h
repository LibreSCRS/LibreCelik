// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "certificatetreeviewmodel.h"

#include <cstdint>
#include <span>

namespace LibreSCRS::Certificate {
class ParsedCertificate;
}

class CertificatePropertiesModel : public CertificateTreeViewModel
{
    Q_OBJECT
public:
    /// @brief Build the properties tree from a parsed certificate.
    /// @param cert    Parsed certificate; if @c nullptr a single "parse error"
    ///                row is inserted so the viewer remains visible.
    /// @param rawDer  Optional raw DER bytes used to render a forensic hex
    ///                dump under the parse-error row when @p cert is null.
    ///                Ignored when @p cert is non-null.
    explicit CertificatePropertiesModel(const LibreSCRS::Certificate::ParsedCertificate* cert,
                                        QObject* parent = nullptr);

    explicit CertificatePropertiesModel(const LibreSCRS::Certificate::ParsedCertificate* cert,
                                        std::span<const std::uint8_t> rawDer, QObject* parent = nullptr);

private:
    void buildTree(const LibreSCRS::Certificate::ParsedCertificate& cert);
    void addParseError(std::span<const std::uint8_t> rawDer);
};
