// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QCoreApplication>
#include <plugin/card_data.h>
#include "textdocument.h"

class HealthTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(HealthTextDocument)

public:
    explicit HealthTextDocument(const plugin::CardData& data, QString documentPath = ":/html/healthcard.html",
                                QString cssPath = ":/html/healthcard.css");

private:
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& html, const plugin::CardData& cardData) const;
};
