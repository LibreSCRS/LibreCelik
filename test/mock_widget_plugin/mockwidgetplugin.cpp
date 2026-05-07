// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "plugin/cardwidgetplugin.h"

#include <QLabel>
#include <QObject>
#include <QtPlugin>

class MockWidgetPlugin : public QObject, public CardWidgetPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.librescrs.CardWidgetPlugin/1.0" FILE "mock-widget.json")
    Q_INTERFACES(CardWidgetPlugin)

public:
    QString cardType() const override
    {
        return "mock";
    }
    QString displayName() const override
    {
        return "Mock Card";
    }

    QWidget* createWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent) const override
    {
        auto* label = new QLabel(QString::fromStdString(data.cardType), parent);
        label->setObjectName("mockLabel");
        return label;
    }
};

#include "mockwidgetplugin.moc"
