// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "asynccardreader.h"
#include "config.h"
#include "plugin/cardwidgetpluginregistry.h"

#include <LibreSCRS/Plugin/CardPluginService.h>
#include <LibreSCRS/SmartCard/MonitorService.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QTranslator>

#include <memory>
#include <stop_token>

#ifdef LIBRECELIK_SIGNING_ENABLED
namespace LibreSCRS::Signing {
class SigningService;
}
namespace LibreSCRS::Trust {
class TrustStoreService;
}
namespace signing {
class SigningConfiguration;
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

signals:
    /// @brief Emitted on every CardRemoved monitor event, BEFORE the
    /// reader's widget + AsyncCardReader are torn down. Subscribers (the
    /// SigningWizard, in particular) filter by readerName and react —
    /// the wizard rejects itself when the card it is signing with goes
    /// away. Emit-before-teardown matters: a slot connected via
    /// `Qt::AutoConnection` from another QObject still owned by this
    /// thread runs synchronously, so the wizard sees the signal before
    /// AsyncCardReader's `disconnect()` clears its session.
    void cardRemoved(QString readerName);

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onCardEventReceived(const LibreSCRS::SmartCard::MonitorEvent& event);
    void onSmartCardReaderEnumerationChanged(const QStringList& scrNames);

private:
    void addNewReader(std::string reader, int retryCount = 0);
    void removeReader(std::string reader);
    void connectPKISignals(AsyncCardReader* reader, QWidget* pkiWidget, const std::string& readerName);
    bool loadLanguage(const QString& locale);
    void updateAboutText();
    void retranslateMenuBar();
    void openSettings();
    void showAboutDialog();

private:
    Ui::LibreCelik* ui;

    // shared_ptr-owned per the LM 4.0 API: CardPluginService no longer
    // supports default construction + setter-DI; it is built at the
    // constructor with the resolved plugin directory. The shared handle
    // also propagates cleanly through the SigningWizard seam without a
    // no-op-deleter aliasing hack.
    //
    // Declaration-order invariant: middlewarePluginRegistry MUST appear
    // BEFORE trustService below. The current LM 4.0 contract injects the
    // TrustStore into each plugin via the registry — plugins capture
    // `shared_ptr<const TrustStore>`, which keeps the underlying store
    // alive past trustService destruction (the TrustStoreService and the
    // TrustStore have independent shared lifetimes). That makes the
    // current order benign. However, a future refactor that flips plugins
    // to a non-owning observer of trustService (e.g. a `weak_ptr<TrustStore>`
    // or a back-pointer to TrustStoreService) would silently break:
    // members are destroyed in reverse declaration order, so trustService
    // would die before plugins released their observer hooks. If/when
    // plugins ever stop holding a strong reference to the TrustStore,
    // swap the declaration order so trustService outlives the registry.
    std::shared_ptr<LibreSCRS::Plugin::CardPluginService> middlewarePluginRegistry;
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
    std::unique_ptr<signing::SigningConfiguration> signingConfiguration;
    // LibreCelik owns a TrustStoreService that lifecycles
    // the unified TrustStore (bundled + system + asynchronously-fetched TL
    // anchors). It is constructed BEFORE signingService so that the
    // signingService can take a shared_ptr<TrustStoreService> via pure ctor
    // DI, and so that destruction order is safe (signingService dies first,
    // releasing its trustService reference; trustService dies last, joining
    // its async fetch workers in its dtor).
    //
    // Declaration order matters: trustService MUST appear before
    // signingService. Members are destroyed in reverse declaration order, so
    // this gives the correct teardown sequence.
    std::shared_ptr<LibreSCRS::Trust::TrustStoreService> trustService;
    // LibreCelik owns a single SigningService instance constructed via pure
    // constructor DI in the LibreCelik ctor (see the `signingService =
    // std::make_shared<...>(...)` call site). The shared_ptr is the LM 4.0
    // API's idiomatic ownership handle — no process-wide cache exists, so
    // this member is the sole strong reference at application scope.
    // In-flight SigningWizards capture their own shared_ptr into the worker
    // lambda so the service outlives any single wizard even if ~LibreCelik
    // runs while a worker is still executing.
    std::shared_ptr<LibreSCRS::Signing::SigningService> signingService;
#endif
};
