// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "certificatetreeviewmodel.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace LibreSCRS::Trust {
class TrustStore;
}

namespace LibreSCRS::Certificate {
class ParsedCertificate;
}

class CertificateHierarchyModel : public CertificateTreeViewModel
{
    Q_OBJECT
public:
    /// @brief Build a chain hierarchy by walking issuers from @p leaf upward
    ///        through the @p trustStore. When @p trustStore is null, only the
    ///        leaf is shown with a "trust unknown" status (graceful
    ///        degradation per Spec §10).
    explicit CertificateHierarchyModel(const LibreSCRS::Certificate::ParsedCertificate* leaf,
                                       std::shared_ptr<const LibreSCRS::Trust::TrustStore> trustStore,
                                       QObject* parent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;

private:
    enum class TrustState {
        Trusted,
        Untrusted,
        Unknown,
    };

    void buildChain(const LibreSCRS::Certificate::ParsedCertificate& leaf);

    std::shared_ptr<const LibreSCRS::Trust::TrustStore> trustStore;
    TrustState trustState = TrustState::Unknown;
};
