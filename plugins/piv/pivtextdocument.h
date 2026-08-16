// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QCoreApplication>
#include <QList>
#include <LibreSCRS/AgentClient/Types.h>
#include "textdocument.h"

class PIVTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(PIVTextDocument)

public:
    explicit PIVTextDocument(const QList<LibreSCRS::AgentClient::FieldGroup>& groups,
                             QString cssPath = ":/html/pivcard.css");

private:
    QString buildHtml(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;

    QString buildChuidSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildCccSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildPrintedSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildDiscoverySection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildKeyHistorySection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;

    QString emitRow(const QString& label, const QString& value) const;
};
