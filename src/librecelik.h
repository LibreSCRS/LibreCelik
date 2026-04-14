// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "asynccardreader.h"
#include "config.h"
#include "plugin/cardwidgetpluginregistry.h"

#include <plugin/card_plugin_registry.h>

namespace smartcard {
struct MonitorEvent;
}

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QTranslator>

#include <memory>
#include <stop_token>

#ifdef LIBRECELIK_SIGNING_ENABLED
namespace libresign {
class SigningService;
}
#endif

class SettingsDialog;

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

private:
    void addNewReader(std::string reader, int retryCount = 0);
    void removeReader(std::string reader);
    void connectPKISignals(AsyncCardReader* reader, QWidget* pkiWidget);
    bool loadLanguage(const QString& locale);
    void updateAboutText();
    void retranslateMenuBar();
    void openSettings();
    void showAboutDialog();

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
    std::map<std::string, std::stop_source> readerStopSource;

    QTranslator translator;
    QTranslator qtTranslator;
    bool uiReady = false;
    QString locale;

    // Menu actions (stored for retranslation)
    QMenu* editMenu = nullptr;
    QMenu* helpMenu = nullptr;
    QAction* settingsAction = nullptr;
    QAction* aboutAction = nullptr;
    QAction* aboutQtAction = nullptr;

#ifdef LIBRECELIK_SIGNING_ENABLED
    std::unique_ptr<libresign::SigningService> signingService;
#endif
};
