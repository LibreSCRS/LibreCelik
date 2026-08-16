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
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& html, const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;

    bool isForeigner = false;
};
