// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "certificatetreeviewmodel.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QByteArrayView>

class CertificatePropertiesModel : public CertificateTreeViewModel
{
    Q_OBJECT
public:
    /// @brief Build the properties tree from one agent-supplied certificate.
    /// @param cert Certificate record; when the agent reports it could not
    ///             parse the certificate, a single "parse error" row is
    ///             inserted so the viewer remains visible.
    explicit CertificatePropertiesModel(const LibreSCRS::AgentClient::CertificateInfo& cert, QObject* parent = nullptr);

    /// @brief As above, plus the raw DER for the forensic hex dump rendered
    ///        under the parse-error row.
    /// @param forensicDer Raw certificate bytes as fetched from the agent.
    ///                    IGNORED unless @p cert is the unparseable kind — a
    ///                    parseable certificate's detail comes from the
    ///                    agent's own fields, and the raw bytes stay
    ///                    available through the viewer's export action.
    CertificatePropertiesModel(const LibreSCRS::AgentClient::CertificateInfo& cert, QByteArrayView forensicDer,
                               QObject* parent = nullptr);

private:
    void buildTree(const LibreSCRS::AgentClient::CertificateInfo& cert);
    void addParseError(const LibreSCRS::AgentClient::CertificateInfo& cert, QByteArrayView forensicDer);
};
