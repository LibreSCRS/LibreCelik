// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef LIBRECELIK_H
#define LIBRECELIK_H

#include "asynccardreader.h"
#include "plugin/cardwidgetpluginregistry.h"

#include <plugin/card_plugin_registry.h>

namespace smartcard {
struct MonitorEvent;
}

#include <QMainWindow>
#include <QTranslator>

QT_BEGIN_NAMESPACE
namespace Ui {
class LibreCelik;
}
QT_END_NAMESPACE

class LibreCelik : public QMainWindow
{
    Q_OBJECT

public:
    LibreCelik(QWidget* parent = nullptr);
    ~LibreCelik();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onCardEventReceived(const smartcard::MonitorEvent& event);
    void onSmartCardReaderEnumerationChanged(const QStringList& scrNames);
    void onLanguageChanged(int index);

private:
    void addNewReader(std::string reader, int retryCount = 0);
    void removeReader(std::string reader);
    void connectPKISignals(AsyncCardReader* reader, QWidget* pkiWidget);
    bool loadLanguage(const QString& locale);
    void updateAboutText();

private:
    Ui::LibreCelik* ui;

    plugin::CardPluginRegistry middlewarePluginRegistry;
    CardWidgetPluginRegistry guiPluginRegistry;

    struct ActiveReader
    {
        AsyncCardReader* reader = nullptr;
        QWidget* widget = nullptr;
    };
    std::map<std::string, ActiveReader> activeReaders;

    QTranslator translator;
    bool uiReady = false;
    QString locale;
};
#endif // LIBRECELIK_H
