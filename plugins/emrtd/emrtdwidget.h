// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EMRTDWIDGET_H
#define EMRTDWIDGET_H

#include <plugin/card_data.h>

#include <QWidget>

class QLabel;
class QLineEdit;

class EMRTDWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EMRTDWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    const plugin::CardData& cardData() const
    {
        return data;
    }

private:
    void showAuthRequired(const plugin::CardFieldGroup* group);
    void showPersonalData(const plugin::CardData& cardData);
    void showError(const plugin::CardFieldGroup* group);

    plugin::CardData data;
};

#endif // EMRTDWIDGET_H
