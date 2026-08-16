// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "certificatetreeviewmodel.h"

#include <LibreSCRS/AgentClient/Types.h>

class CertificateHierarchyModel : public CertificateTreeViewModel
{
    Q_OBJECT
public:
    /// @brief Render the certification path the agent resolved.
    ///
    /// The path is display data: `CertificateInfo::chainSubjectCns` arrives
    /// ordered leaf..root and is shown root-first, nested. This model never
    /// walks or validates a chain of its own — the verdict is the agent's,
    /// carried on `trust` / `securityStatus`, and only the leaf row shows it.
    /// An agent that resolved no path leaves a single leaf node.
    explicit CertificateHierarchyModel(const LibreSCRS::AgentClient::CertificateInfo& cert, QObject* parent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;

private:
    void buildChain(const LibreSCRS::AgentClient::CertificateInfo& cert);

    LibreSCRS::AgentClient::TrustStatus trust = LibreSCRS::AgentClient::TrustStatus::Unknown;
};
