// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QCoreApplication>
#include <QList>
#include <LibreSCRS/AgentClient/Types.h>
#include "textdocument.h"

class EuVrcTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(EuVrcTextDocument)

public:
    explicit EuVrcTextDocument(const QList<LibreSCRS::AgentClient::FieldGroup>& groups,
                               QString cssPath = ":/html/euvrccard.css");

private:
    QString buildHtml(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;

    QString buildRegistrationSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildVehicleSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildEngineTechnicalSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildHolderSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildOwnerSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildUserSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;
    QString buildNationalSection(const QList<LibreSCRS::AgentClient::FieldGroup>& groups) const;

    // Emit a table row with one label-value pair. Returns empty if value is empty.
    // Optional cssClass applied to the value <td>.
    QString emitRow(const QString& label, const QString& value, const QString& cssClass = {}) const;
};
