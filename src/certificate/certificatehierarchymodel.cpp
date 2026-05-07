// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificatehierarchymodel.h"
#include "certificateinfoitem.h"
#include "utils/libreceliklog.h"

#include <LibreSCRS/Certificate/ParsedCertificate.h>
#include <LibreSCRS/Trust/TrustStore.h>

#include <QIcon>

#include <algorithm>
#include <span>

namespace lcc = LibreSCRS::Certificate;
namespace lct = LibreSCRS::Trust;

CertificateHierarchyModel::CertificateHierarchyModel(const lcc::ParsedCertificate* leaf,
                                                     std::shared_ptr<const lct::TrustStore> store, QObject* parent)
    : CertificateTreeViewModel(parent), trustStore(std::move(store))
{
    if (leaf)
        buildChain(*leaf);
}

QVariant CertificateHierarchyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (role == Qt::DecorationRole && index.column() == 0) {
        auto* item = static_cast<CertificateInfoItem*>(index.internalPointer());
        // Only the leaf gets a status icon (it owns the verification result).
        if (item->childCount() == 0) {
            switch (trustState) {
            case TrustState::Trusted:
                return QIcon(QStringLiteral(":/images/green_checked.png"));
            case TrustState::Untrusted:
                return QIcon(QStringLiteral(":/images/red_exclamation.png"));
            case TrustState::Unknown:
                // No icon — UI shows "trust unknown" text in column 1.
                return {};
            }
        }
    }

    return CertificateTreeViewModel::data(index, role);
}

void CertificateHierarchyModel::buildChain(const lcc::ParsedCertificate& leaf)
{
    // Walk issuers up the chain via TrustStore::findIssuerOf. Stop when we
    // reach a self-signed cert (issuer not found and CN of issuer == subject)
    // or when findIssuerOf returns nullopt and the cert is not self-issued.
    //
    // The walk holds parsed copies (we need the DER for the next lookup).

    std::vector<lcc::ParsedCertificate> chain;
    chain.reserve(8);

    // Add leaf — copy its DER so we own the byte block for the lifetime of
    // the function (ParsedCertificate is move-only).
    {
        const auto leafDer = leaf.derBytes();
        std::vector<std::uint8_t> derCopy(leafDer.begin(), leafDer.end());
        if (auto parsed = lcc::ParsedCertificate::fromDer(derCopy))
            chain.push_back(std::move(*parsed));
    }

    if (trustStore) {
        // Walk upward up to a sane bound (chains rarely exceed 10).
        constexpr std::size_t kMaxChain = 16;
        while (chain.size() < kMaxChain) {
            const auto& current = chain.back();
            const auto curDer = current.derBytes();
            auto issuer = trustStore->findIssuerOf(curDer);
            if (!issuer)
                break;
            auto parsed = lcc::ParsedCertificate::fromDer(issuer->certificateDer);
            if (!parsed)
                break;
            // Self-signed root reached — TrustStore::findIssuerOf returns the
            // cert itself for self-signed roots present in the trust store.
            // Test BEFORE pushing so we don't duplicate the root (or, when the
            // leaf itself is a self-signed root, end up with [leaf, leaf]).
            // Use the DER bytes as the identity check (cheap, robust against
            // re-encoded DNs).
            const auto parsedDer = parsed->derBytes();
            const bool sameAsCurrent =
                parsedDer.size() == curDer.size() && std::equal(parsedDer.begin(), parsedDer.end(), curDer.begin());
            if (sameAsCurrent)
                break; // self-signed root already in chain — stop without duplicate
            chain.push_back(std::move(*parsed));
        }
    }

    // Validate the chain once (leaf-first views) and derive both the UI status
    // text and the icon trust-state from the same outcome.
    trustState = TrustState::Unknown;
    QString statusString = qtTrId("lc-cert-verify-trust-unknown");
    if (trustStore && !chain.empty()) {
        std::vector<lct::CertificateView> views;
        views.reserve(chain.size());
        for (const auto& c : chain)
            views.emplace_back(c.derBytes());
        const auto status = trustStore->validateChain(views);
        qCDebug(lcCertificates) << "Chain validation status =" << static_cast<int>(status);
        switch (status) {
        case lct::TrustStore::ChainStatus::Trusted:
            trustState = TrustState::Trusted;
            statusString = qtTrId("lc-cert-verify-valid");
            break;
        case lct::TrustStore::ChainStatus::UntrustedRoot:
            trustState = TrustState::Untrusted;
            statusString = qtTrId("lc-cert-verify-untrusted-root");
            break;
        case lct::TrustStore::ChainStatus::BrokenChain:
            trustState = TrustState::Untrusted;
            statusString = qtTrId("lc-cert-verify-no-issuer");
            break;
        case lct::TrustStore::ChainStatus::InvalidCertificate:
            trustState = TrustState::Untrusted;
            statusString = qtTrId("lc-cert-verify-unspecified-error");
            break;
        case lct::TrustStore::ChainStatus::Expired:
            trustState = TrustState::Untrusted;
            statusString = qtTrId("lc-cert-verify-expired");
            break;
        }
    }

    // Build the tree from root (last in chain) to leaf (first).
    CertificateInfoItem* current = rootItem.get();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        QString label = QString::fromStdString(it->subject().commonName());
        if (label.isEmpty())
            label = qtTrId("lc-cert-unknown");

        const bool isLeaf = (it == chain.rend() - 1);
        QString value = isLeaf ? statusString : QString();

        auto item = std::make_unique<CertificateInfoItem>(label, value, current);
        auto* ptr = item.get();
        current->appendChild(std::move(item));
        current = ptr;
    }
}
