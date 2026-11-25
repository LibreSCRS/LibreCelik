// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QCoreApplication>
#include <QList>
#include <LibreSCRS/AgentClient/Types.h>
#include "textdocument.h"

class EIdTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(EIdTextDocument)

public:
    explicit EIdTextDocument(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QString documentPath = {},
                             QString cssPath = {});

private:
    const char* titleId() const;
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& html, const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;

    // "ID" / "IF" / "RP" / "EL"; anything else prints as the citizen card.
    QString code;
    bool isForeigner = false;
};
