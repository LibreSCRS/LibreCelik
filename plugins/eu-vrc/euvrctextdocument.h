// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QCoreApplication>
#include <LibreSCRS/Plugin/CardData.h>
#include "textdocument.h"

class EuVrcTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(EuVrcTextDocument)

public:
    explicit EuVrcTextDocument(const LibreSCRS::Plugin::CardData& data, QString cssPath = ":/html/euvrccard.css");

private:
    QString buildHtml(const LibreSCRS::Plugin::CardData& cardData) const;

    QString buildRegistrationSection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildVehicleSection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildEngineTechnicalSection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildHolderSection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildOwnerSection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildUserSection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildNationalSection(const LibreSCRS::Plugin::CardData& cardData) const;

    // Emit a table row with one label-value pair. Returns empty if value is empty.
    // Optional cssClass applied to the value <td>.
    QString emitRow(const QString& label, const QString& value, const QString& cssClass = {}) const;
};
