// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QCoreApplication>
#include <plugin/card_data.h>
#include "textdocument.h"

class VehicleTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(VehicleTextDocument);

public:
    explicit VehicleTextDocument(const plugin::CardData& data, QString documentPath = ":/html/vehiclecard.html",
                                 QString cssPath = ":/html/vehiclecard.css");

private:
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& html, const plugin::CardData& cardData) const;
};
