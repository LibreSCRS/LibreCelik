// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QCoreApplication>
#include <LibreSCRS/Plugin/CardData.h>
#include "textdocument.h"

class HealthTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(HealthTextDocument)

public:
    explicit HealthTextDocument(const LibreSCRS::Plugin::CardData& data,
                                QString documentPath = ":/html/healthcard.html",
                                QString cssPath = ":/html/healthcard.css");

private:
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& html, const LibreSCRS::Plugin::CardData& cardData) const;
};
