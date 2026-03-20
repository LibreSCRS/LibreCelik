// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef PKSWIDGET_H
#define PKSWIDGET_H

#include <plugin/card_data.h>
#include <QWidget>

class PksWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PksWidget(const plugin::CardData& data, QWidget* parent = nullptr);
};

#endif // PKSWIDGET_H
