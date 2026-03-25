// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QCoreApplication>
#include <plugin/card_data.h>
#include "textdocument.h"

class EIdTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(EIdTextDocument)

public:
    explicit EIdTextDocument(const plugin::CardData& data, QString documentPath = {}, QString cssPath = {});

private:
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& html, const plugin::CardData& cardData) const;

    bool isForeigner = false;
};
