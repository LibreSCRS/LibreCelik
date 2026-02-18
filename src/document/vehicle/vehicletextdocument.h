// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef VEHICLETEXTDOCUMENT_H
#define VEHICLETEXTDOCUMENT_H

#include <QCoreApplication>
#include <vehiclecard/vehicletypes.h>
#include "document/textdocument.h"

class VehicleTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(VehicleTextDocument);

public:
    VehicleTextDocument(const vehiclecard::VehicleDocumentData& vehicleData,
                        QString documentPath = ":/html/vehiclecard.html",
                        QString cssPath = ":/html/vehiclecard.css");

private:
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& data, const vehiclecard::VehicleDocumentData& vehicleData) const;
};

#endif // VEHICLETEXTDOCUMENT_H
