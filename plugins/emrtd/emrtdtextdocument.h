// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QCoreApplication>
#include <plugin/card_data.h>
#include "textdocument.h"

class EMRTDTextDocument : public TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(EMRTDTextDocument)

public:
    explicit EMRTDTextDocument(const plugin::CardData& data, QString documentPath = {}, QString cssPath = {});

private:
    void translateDocumentData(QString& data) const;
    void prepareDocumentData(QString& html, const plugin::CardData& cardData) const;
    void removeConditionalBlock(QString& html, const QString& marker) const;
};
