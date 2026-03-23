// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <plugin/card_data.h>
#include <QWidget>

class QVBoxLayout;

class PksWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PksWidget(const plugin::CardData& data, QWidget* parent = nullptr);
    explicit PksWidget(QWidget* parent = nullptr);

    void addGroup(const plugin::CardFieldGroup& group);

    const plugin::CardData& cardData() const
    {
        return data;
    }

private:
    void buildHeader();

    plugin::CardData data;
    QVBoxLayout* outerLayout = nullptr;
    bool headerBuilt = false;
};
