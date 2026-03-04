// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef HEALTHTEXTDOCUMENT_H
#define HEALTHTEXTDOCUMENT_H

#include <QCoreApplication>
#include <healthcard/healthtypes.h>
#include "document/textdocument.h"

class HealthTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(HealthTextDocument);

public:
    HealthTextDocument(const healthcard::HealthDocumentData& healthData,
                       QString documentPath = ":/html/healthcard.html",
                       QString cssPath = ":/html/healthcard.css");

private:
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& data, const healthcard::HealthDocumentData& healthData) const;
};

#endif // HEALTHTEXTDOCUMENT_H
