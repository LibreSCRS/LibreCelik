// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QCoreApplication>
#include <QList>
#include <LibreSCRS/AgentClient/Types.h>
#include "textdocument.h"

class EMRTDTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(EMRTDTextDocument)

public:
    explicit EMRTDTextDocument(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QString documentPath = {},
                               QString cssPath = {});

private:
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& html, const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    void removeConditionalBlock(QString& html, const QString& marker) const;
};
