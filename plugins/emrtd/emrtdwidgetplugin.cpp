// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdwidgetplugin.h"
#include "emrtdwidget.h"
#include "emrtdtextdocument.h"
#include "utils/printmanager.h"

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
    auto* w = new EMRTDWidget(data, parent);
    connect(w, &EMRTDWidget::printRequested, this, [this](const plugin::CardData& d) { print(d); });
    return w;
}

QWidget* EMRTDWidgetPlugin::createEmptyWidget(QWidget* parent) const
{
    auto* w = new EMRTDWidget(parent);
    connect(w, &EMRTDWidget::printRequested, this, [this](const plugin::CardData& d) { print(d); });
    return w;
}

void EMRTDWidgetPlugin::addGroup(const plugin::CardFieldGroup& group, QWidget* widget) const
{
    if (auto* w = qobject_cast<EMRTDWidget*>(widget))
        w->addGroup(group);
}

void EMRTDWidgetPlugin::showNoDataMessage(QWidget* widget) const
{
    if (auto* w = qobject_cast<EMRTDWidget*>(widget))
        w->showNoDataMessage();
}

void EMRTDWidgetPlugin::print(const plugin::CardData& data) const
{
    EMRTDTextDocument doc(data);
    PrintManager::printDocument(doc, qtTrId("lc-emrtd-doc-title"));
}
