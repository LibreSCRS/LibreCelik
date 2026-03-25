// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QCoreApplication>
#include <plugin/card_data.h>
#include "textdocument.h"

class EuVrcTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(EuVrcTextDocument)

public:
    explicit EuVrcTextDocument(const plugin::CardData& data, QString cssPath = ":/html/euvrccard.css");

private:
    QString buildHtml(const plugin::CardData& cardData) const;

    QString buildRegistrationSection(const plugin::CardData& cardData) const;
    QString buildVehicleSection(const plugin::CardData& cardData) const;
    QString buildEngineTechnicalSection(const plugin::CardData& cardData) const;
    QString buildHolderSection(const plugin::CardData& cardData) const;
    QString buildOwnerSection(const plugin::CardData& cardData) const;
    QString buildUserSection(const plugin::CardData& cardData) const;
    QString buildNationalSection(const plugin::CardData& cardData) const;

    // Emit a table row with one label-value pair. Returns empty if value is empty.
    // Optional cssClass applied to the value <td>.
    QString emitRow(const QString& label, const QString& value, const QString& cssClass = {}) const;
};
