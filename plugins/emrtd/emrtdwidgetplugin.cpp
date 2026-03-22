// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdwidgetplugin.h"
#include "emrtdwidget.h"

#include <QLabel>
#include <QVBoxLayout>

QWidget* EMRTDWidgetPlugin::createWidget(const plugin::CardData& data, QWidget* parent) const
{
    if (data.cardType == "pkcs15") {
        // PKI-only mode — minimal widget, TokenSection will be added by main window
        auto* widget = new QWidget(parent);
        auto* layout = new QVBoxLayout(widget);
        auto* label = new QLabel(qtTrId("lc-pkcs15-smart-card"), widget);
        label->setStyleSheet("font-size: 16px; font-weight: bold; color: #226E75;");
        layout->addWidget(label);
        layout->addStretch();
        return widget;
    }
    return new EMRTDWidget(data, parent);
}
