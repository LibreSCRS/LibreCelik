// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QCoreApplication>
#include <plugin/card_data.h>
#include "textdocument.h"

class PIVTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(PIVTextDocument)

public:
    explicit PIVTextDocument(const plugin::CardData& data, QString cssPath = ":/html/pivcard.css");

private:
    QString buildHtml(const plugin::CardData& cardData) const;

    QString buildChuidSection(const plugin::CardData& cardData) const;
    QString buildCccSection(const plugin::CardData& cardData) const;
    QString buildPrintedSection(const plugin::CardData& cardData) const;
    QString buildDiscoverySection(const plugin::CardData& cardData) const;
    QString buildKeyHistorySection(const plugin::CardData& cardData) const;

    QString emitRow(const QString& label, const QString& value) const;
};
