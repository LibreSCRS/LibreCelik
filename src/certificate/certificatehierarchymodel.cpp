// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificatehierarchymodel.h"
#include "certificateinfoitem.h"

#include <QIcon>
#include <QStringList>

using LibreSCRS::AgentClient::CertificateInfo;
using LibreSCRS::AgentClient::TrustStatus;

namespace {

/// LC copy for one security-status token.
///
/// Same known-token/verbatim rule the token section applies: a token this
/// build names renders LC's own string, anything else is displayed exactly as
/// it arrived. The vocabulary is the agent's and grows independently of this
/// build.
[[nodiscard]] QString securityStatusText(const QString& token)
{
    if (token == QLatin1StringView("trusted"))
        return qtTrId("lc-cert-status-trusted");
    if (token == QLatin1StringView("untrusted-root"))
        return qtTrId("lc-cert-status-untrusted-root");
    if (token == QLatin1StringView("broken-chain"))
        return qtTrId("lc-cert-status-broken-chain");
    if (token == QLatin1StringView("invalid"))
        return qtTrId("lc-cert-status-invalid");
    if (token == QLatin1StringView("expired"))
        return qtTrId("lc-cert-status-expired");
    if (token == QLatin1StringView("revoked"))
        return qtTrId("lc-cert-status-revoked");
    if (token == QLatin1StringView("offline-unverified"))
        return qtTrId("lc-cert-status-offline-unverified");
    return token;
}

/// The status the leaf row shows: every token the agent reported, in its own
/// order. An agent that reported none says nothing about trust, which is what
/// the row then says.
[[nodiscard]] QString chainStatusText(const CertificateInfo& cert)
{
    if (cert.securityStatus.isEmpty())
        return qtTrId("lc-cert-verify-trust-unknown");
    QStringList parts;
    parts.reserve(cert.securityStatus.size());
    for (const QString& token : cert.securityStatus)
        parts << securityStatusText(token);
    return parts.join(QStringLiteral(" · "));
}

} // namespace

CertificateHierarchyModel::CertificateHierarchyModel(const CertificateInfo& cert, QObject* parent)
    : CertificateTreeViewModel(parent), trust(cert.trust)
{
    buildChain(cert);
}

QVariant CertificateHierarchyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (role == Qt::DecorationRole && index.column() == 0) {
        auto* item = static_cast<CertificateInfoItem*>(index.internalPointer());
        // Only the leaf gets a status icon (it owns the verification result).
        // Unknown is deliberately unadorned: it means no verdict was
        // available, not a negative one.
        if (item->childCount() == 0) {
            switch (trust) {
            case TrustStatus::Trusted:
                return QIcon(QStringLiteral(":/images/green_checked.png"));
            case TrustStatus::Untrusted:
            case TrustStatus::Revoked:
            case TrustStatus::Expired:
                return QIcon(QStringLiteral(":/images/red_exclamation.png"));
            case TrustStatus::Unknown:
                return {};
            }
        }
    }

    return CertificateTreeViewModel::data(index, role);
}

void CertificateHierarchyModel::buildChain(const CertificateInfo& cert)
{
    // The agent orders the path leaf..root; the tree nests root-first, so the
    // walk runs backwards. With no path resolved there is still exactly one
    // node to show — this certificate.
    QStringList names = cert.chainSubjectCns;
    if (names.isEmpty())
        names << cert.subject;

    const QString statusString = chainStatusText(cert);

    CertificateInfoItem* current = rootItem.get();
    for (auto it = names.crbegin(); it != names.crend(); ++it) {
        QString label = *it;
        // clang-format off
        if (label.isEmpty())
            label = qtTrId("lc-cert-unknown"); // i18n-audit: ignore D1, item-view model retranslates via Qt::DisplayRole on next paint
        // clang-format on

        const bool isLeaf = (it == names.crend() - 1);
        auto item = std::make_unique<CertificateInfoItem>(label, isLeaf ? statusString : QString(), current);
        auto* ptr = item.get();
        current->appendChild(std::move(item));
        current = ptr;
    }
}
