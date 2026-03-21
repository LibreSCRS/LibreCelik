// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EMRTDWIDGET_H
#define EMRTDWIDGET_H

#include <plugin/card_data.h>

#include <QWidget>

class QLabel;
class QLineEdit;
class QFormLayout;
class QGroupBox;

class EMRTDWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EMRTDWidget(const plugin::CardData& data, QWidget* parent = nullptr);

private:
    void showAuthRequired(const plugin::CardFieldGroup* group);
    void showPersonalData(const plugin::CardData& data);
    void showError(const plugin::CardFieldGroup* group);
    void addField(QFormLayout* layout, const QString& label, const QString& value);
};

#endif // EMRTDWIDGET_H
