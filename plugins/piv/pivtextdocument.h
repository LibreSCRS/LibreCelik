// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QCoreApplication>
#include <LibreSCRS/Plugin/CardData.h>
#include "textdocument.h"

class PIVTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(PIVTextDocument)

public:
    explicit PIVTextDocument(const LibreSCRS::Plugin::CardData& data, QString cssPath = ":/html/pivcard.css");

private:
    QString buildHtml(const LibreSCRS::Plugin::CardData& cardData) const;

    QString buildChuidSection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildCccSection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildPrintedSection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildDiscoverySection(const LibreSCRS::Plugin::CardData& cardData) const;
    QString buildKeyHistorySection(const LibreSCRS::Plugin::CardData& cardData) const;

    QString emitRow(const QString& label, const QString& value) const;
};
