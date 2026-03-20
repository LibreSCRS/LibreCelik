// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDWIDGET_H
#define EIDWIDGET_H

#include <plugin/card_data.h>
#include <QWidget>

class QLabel;
class QResizeEvent;

namespace Ui {
class EId;
}

class EidWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EidWidget(const plugin::CardData& data, QWidget* parent = nullptr);
    ~EidWidget();

    const plugin::CardData& cardData() const
    {
        return data;
    }

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void populatePersonalData(const plugin::CardFieldGroup* group);
    void populateAddressData(const plugin::CardFieldGroup* group);
    void populateDocumentData(const plugin::CardFieldGroup* group);
    void populatePhoto(const plugin::CardFieldGroup* group);
    void applyCardTypeVisibility();
    void repositionAddressSection();
    QString assembleAddress(const plugin::CardFieldGroup* group) const;
    QString assemblePlaceOfBirth(const plugin::CardFieldGroup* group) const;

    Ui::EId* ui;
    plugin::CardData data;
    bool isForeigner = false;
    bool addressInColumn = false;
};

#endif // EIDWIDGET_H
