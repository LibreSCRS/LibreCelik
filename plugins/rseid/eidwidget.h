// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDWIDGET_H
#define EIDWIDGET_H

#include <plugin/card_data.h>
#include <QWidget>

class CollapsibleSection;
class QVBoxLayout;

namespace LibreSCRS {
class CardHeaderCard;
}

class EidWidget : public QWidget
{
    Q_OBJECT
public:
    // Full-data constructor (existing behaviour, unchanged)
    explicit EidWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    // Empty-shell constructor for progressive display
    explicit EidWidget(QWidget* parent);

    // Progressive display: add one group at a time
    void addGroup(const plugin::CardFieldGroup& group);

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

    // Progressive-display state
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* sectionLayout = nullptr;
    LibreSCRS::CardHeaderCard* headerCard = nullptr;
};

#endif // EIDWIDGET_H
