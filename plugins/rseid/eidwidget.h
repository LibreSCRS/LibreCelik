// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDWIDGET_H
#define EIDWIDGET_H

#include <plugin/card_data.h>
#include <QWidget>

class CollapsibleSection;

class EidWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EidWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    const plugin::CardData& cardData() const
    {
        return data;
    }

private:
    void buildLayout();
    bool isForeigner() const;
    QPixmap loadPhoto() const;
    CollapsibleSection* buildAddressSection(QWidget* parent) const;
    CollapsibleSection* buildDocumentSection(QWidget* parent) const;

    plugin::CardData data;
};

#endif // EIDWIDGET_H
