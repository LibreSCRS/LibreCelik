// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef HEALTHWIDGET_H
#define HEALTHWIDGET_H

#include <plugin/card_data.h>
#include <QWidget>

class CollapsibleSection;

class HealthWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HealthWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    const plugin::CardData& cardData() const
    {
        return data;
    }

private:
    void buildLayout();
    void transformPermanentlyValid();

    plugin::CardData data;
    CollapsibleSection* carrierSection = nullptr;
};

#endif // HEALTHWIDGET_H
