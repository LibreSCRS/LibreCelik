// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CELIKTEXTDOCUMENT_H
#define CELIKTEXTDOCUMENT_H

#include <QPrinter>
#include <QTextDocument>
#include <QCoreApplication>
#include "celikapi/celikapiplus.h"

class QTranslator;

class EIdTextDocument
{
    Q_DECLARE_TR_FUNCTIONS(CelikTextDocument);

public:
    EIdTextDocument(const CelikAPI::FixedPersonalData& fixedPersonalData,
                      const CelikAPI::VariablePersonalData& variablePersonalData,
                      const CelikAPI::DocumentData& documentData,
                      const QString& photo,
                      QString documentPath = ":/html/idcard.html",
                      QString cssPath = ":/html/idcard.css");
    void print(QPrinter *printer) const;

protected:
    QTextDocument document;

    void prepareDocumentData(QString &data, const CelikAPI::FixedPersonalData& fixedPersonalData,
                             const CelikAPI::VariablePersonalData& variablePersonalData,
                             const CelikAPI::DocumentData& documentData,
                             const QString& photo) const;
    void translateDocumentData(QString& data) const;
    QString getPreparedValue(const QString& data) const;
    
    bool isMacOS() const;
    bool isLinux() const;
    inline static const std::string MACOS = "Darwin";
    inline static const std::string LINUX = "Linux";
    inline static const int MACOS_FONTSIZE = 12;
    inline static const int LINUX_FONTSIZE = 8;
private:
    bool isForeigner = false;
};

#endif // CELIKTEXTDOCUMENT_H
