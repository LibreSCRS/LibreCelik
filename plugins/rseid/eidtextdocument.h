// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CELIKTEXTDOCUMENT_H
#define CELIKTEXTDOCUMENT_H

#include <QCoreApplication>
#include <eidcard/eidtypes.h>
#include "textdocument.h"

class EIdTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(CelikTextDocument);

public:
    EIdTextDocument(const eidcard::FixedPersonalData& fixedPersonalData,
                    const eidcard::VariablePersonalData& variablePersonalData,
                    const eidcard::DocumentData& documentData, const QString& address, const QString& placeOfBirth,
                    const QString& photo, QString documentPath = ":/html/idcard.html",
                    QString cssPath = ":/html/idcard.css");

private:
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& data, const eidcard::FixedPersonalData& fixedPersonalData,
                             const eidcard::VariablePersonalData& variablePersonalData,
                             const eidcard::DocumentData& documentData, const QString& address,
                             const QString& placeOfBirth, const QString& photo) const;

    bool isForeigner = false;
};

#endif // CELIKTEXTDOCUMENT_H
