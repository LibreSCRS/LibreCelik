// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "librecelik.h"
#include "aboutdialog.h"
#include "agent/agentstatewidget.h"
#include "agent/cardcontroller.h"
#include "agent/live/liveagentgateway.h"
#include "agent/optionalsections.h"
#include "agent/plugintyperesolution.h"
#include "agent/settingsimport.h"
#include "certificate/certificateviewerdlg.h"
#include "config.h"
#include "document/rs-eid/changepindlg.h"
#include "document/tokensection.h"
#include "settings/settingsdialog.h"
#include "settings/settingskeys.h"
#include "ui_librecelik.h"
#include "utils/libreceliklog.h"

#include <LibreSCRS/AgentClient/AgentCapabilities.h>

#ifdef LIBRECELIK_SIGNING_ENABLED
#include "signing/signingwizard.h"
#endif

#ifdef Q_OS_MACOS
#include "utils/macos_menu.h"
#endif

#include "utils/localeresolver.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QEvent>
#include <QLabel>
#include <QLibraryInfo>
#include <QLocale>
#include <QMenu>
#include <QProgressBar>
#include <QScrollArea>
#include <QSettings>
#include <QStringList>
#include <QVBoxLayout>

using librecelik::agent::AgentGateway;
using librecelik::agent::CardController;
using librecelik::agent::PresenceState;
using LibreSCRS::AgentClient::CertificateInfo;
using LibreSCRS::AgentClient::CredentialList;
using LibreSCRS::AgentClient::FieldGroup;
using LibreSCRS::AgentClient::UiState;
namespace Cap = LibreSCRS::AgentClient::Cap;

namespace {

/// The feature token that says the agent can name a card's type at all. An
/// agent without it never sends one, which is a different situation from a
/// card whose type has not resolved YET.
constexpr QLatin1StringView kCardTypeFeature{"card-type"};
// (the generic token page key lives in plugintyperesolution.h with the
// decision that picks it)

/// Display form of the ATR the agent reports for an unrecognised card: the
/// leading @p maxBytes bytes, space-separated and upper-case, with a marker
/// when the ATR is longer.
///
/// The middleware read path built this out of the raw ATR bytes; the agent
/// hands the ATR over as a flat hex string, so the same display shape is a
/// regrouping of characters rather than a formatting of bytes. An odd-length
/// input (which the client contract does not produce) keeps its trailing nibble
/// rather than silently dropping it.
[[nodiscard]] QString atrSnippet(const QString& atrHex, qsizetype maxBytes = 6)
{
    QStringList bytes;
    for (qsizetype offset = 0; offset < atrHex.size() && bytes.size() < maxBytes; offset += 2)
        bytes << atrHex.mid(offset, 2).toUpper();
    QString out = bytes.join(QLatin1Char(' '));
    if (bytes.size() * 2 < atrHex.size())
        out += QStringLiteral(" ...");
    return out;
}

/// Whether @p widget is the read-in-progress placeholder rather than a built
/// card page.
[[nodiscard]] bool isSpinner(QWidget* widget)
{
    return widget != nullptr && widget->property("isSpinner").toBool();
}

/// Group keys that carry no user-visible card data: the read's own
/// bookkeeping, and the PKI material the token section renders instead.
[[nodiscard]] bool isVisibleDataGroup(const FieldGroup& group)
{
    if (group.fields.isEmpty())
        return false;
    return group.key != QLatin1StringView("error") && group.key != QLatin1StringView("presence") &&
           group.key != QLatin1StringView("token") && group.key != QLatin1StringView("meta") &&
           group.key != QLatin1StringView("certificates") && group.key != QLatin1StringView("pins");
}

[[nodiscard]] bool hasVisibleData(const QList<FieldGroup>& groups)
{
    return std::any_of(groups.begin(), groups.end(), isVisibleDataGroup);
}

/// The read-in-progress page. @p text names what is being waited for — a card
/// read, or the holder's answer in the agent's own dialog.
[[nodiscard]] QWidget* makeSpinnerPage(const QString& text, QWidget* parent)
{
    auto* spinnerWidget = new QWidget(parent);
    spinnerWidget->setProperty("isSpinner", true);
    auto* layout = new QVBoxLayout(spinnerWidget);
    layout->setAlignment(Qt::AlignCenter);
    auto* bar = new QProgressBar(spinnerWidget);
    bar->setRange(0, 0);
    bar->setFixedWidth(200);
    bar->setTextVisible(false);
    auto* label = new QLabel(text, spinnerWidget);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    layout->addWidget(bar);
    layout->addWidget(label);
    return spinnerWidget;
}

/// The card page shell every built page shares: a scroll area over a top-
/// aligned column, @p content first.
[[nodiscard]] QScrollArea* makeCardPage(QWidget* content, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setAlignment(Qt::AlignTop);
    containerLayout->addWidget(content);

    auto* scrollArea = new QScrollArea(parent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(container);
    return scrollArea;
}

} // namespace

LibreCelik::LibreCelik(QWidget* parent) : QMainWindow(parent), ui(new Ui::LibreCelik)
{
    qCDebug(lcGeneral, "Setting up GUI");

    // Install translator BEFORE setupUi so the initial UI render is translated.
    // changeEvent is guarded by uiReady to avoid calling retranslateUi before
    // setupUi has run.
    QSettings settings(settings::kOrganization, settings::kApplication);
    const QString resolved = utils::resolveActiveLocale(settings.value(settings::kLanguage, QString()).toString(),
                                                        utils::supportedLocaleCodes(), QLocale::system().uiLanguages());
    loadLanguage(resolved);

    ui->setupUi(this);
    uiReady = true;

    // Load the GUI plugins. In deployed packages (AppImage, DMG) they live next
    // to the executable; fall back to the build-tree path for development.
    // There is no middleware plugin directory to resolve any more — the agent
    // owns the card drivers, and LC only renders what they produce.
    auto resolvePluginDir = [](const QString& subdir, const char* buildFallback) -> QString {
        QDir appDir(QCoreApplication::applicationDirPath());
#ifdef Q_OS_MACOS
        // .app/Contents/MacOS/../PlugIns/<subdir>
        QDir bundleDir(appDir.filePath("../PlugIns/" + subdir));
#else
        // AppImage: usr/bin/../lib/<subdir>
        QDir bundleDir(appDir.filePath("../lib/" + subdir));
#endif
        if (bundleDir.exists())
            return bundleDir.absolutePath();
        return QString::fromUtf8(buildFallback);
    };
    guiPluginRegistry.loadPluginsFromDirectory(resolvePluginDir("gui-plugins", LIBRECELIK_GUI_PLUGIN_DIR));

    ui->stackedWidget->setCurrentIndex(0);

    // Reader selection: cancel the read of the card being navigated away from
    // before the switch. A page the user has left must not keep its card busy —
    // this is what the middleware path's per-reader stop source used to do.
    connect(ui->readerComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        const int leaving = ui->readerStackedWidget->currentIndex();
        if (leaving >= 0 && leaving != index) {
            for (const auto& [cardId, page] : activeCards) {
                if (ui->readerStackedWidget->indexOf(page) == leaving) {
                    if (auto* controller = gateway->cardController(cardId))
                        controller->cancel();
                    break;
                }
            }
        }
        ui->readerStackedWidget->setCurrentIndex(index);
    });

    ui->statusbar->hide();
    // Menu bar. On macOS the Edit menu is omitted entirely — LC has no edit
    // actions of its own, and QAction::PreferencesRole moves Settings into the
    // application menu, so the Edit menu would otherwise be left empty (filled
    // only with macOS-auto-injected Dictation/Emoji entries). Settings is then
    // parented to helpMenu purely so the role-based promotion has an anchor;
    // it is moved out to the application menu at runtime.
#ifndef Q_OS_MACOS
    editMenu = ui->menubar->addMenu(qtTrId("lc-menu-edit"));
    settingsAction = editMenu->addAction(qtTrId("lc-menu-settings"));
#endif
    helpMenu = ui->menubar->addMenu(qtTrId("lc-menu-help"));
#ifdef Q_OS_MACOS
    settingsAction = helpMenu->addAction(qtTrId("lc-menu-settings"));
#endif
    settingsAction->setMenuRole(QAction::PreferencesRole);
    settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(settingsAction, &QAction::triggered, this, &LibreCelik::openSettings);

    aboutAction = helpMenu->addAction(qtTrId("lc-menu-about"));
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, &LibreCelik::showAboutDialog);
    aboutQtAction = helpMenu->addAction(qtTrId("lc-menu-about-qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
#ifdef Q_OS_MACOS
    macosRetranslateAppMenu({.about = qtTrId("lc-menu-about"),
                             .preferences = qtTrId("lc-menu-settings"),
                             .services = qtTrId("lc-menu-services"),
                             .hide = qtTrId("lc-menu-hide"),
                             .hideOthers = qtTrId("lc-menu-hide-others"),
                             .showAll = qtTrId("lc-menu-show-all"),
                             .quit = qtTrId("lc-menu-quit")});
#endif

    // Auto-hide the status bar once its message is cleared (e.g. after showMessage timeout
    // or explicit clearMessage). This keeps the bar invisible except when in use.
    connect(ui->statusbar, &QStatusBar::messageChanged, this, [this](const QString& msg) {
        if (msg.isEmpty())
            ui->statusbar->hide();
    });

    gateway = std::make_unique<librecelik::agent::LiveAgentGateway>();
    connect(gateway.get(), &AgentGateway::presenceChanged, this, &LibreCelik::onPresenceChanged);
    // The banner names the agent's version, which is only knowable while the
    // agent answers — so it is re-rendered on every presence move.
    connect(gateway.get(), &AgentGateway::presenceChanged, this, &LibreCelik::updateAboutText);
    connect(gateway.get(), &AgentGateway::readersChanged, this, &LibreCelik::onReadersChanged);
    connect(gateway.get(), &AgentGateway::cardChanged, this, &LibreCelik::onCardChanged);
    connect(gateway.get(), &AgentGateway::cardRemoved, this, &LibreCelik::onCardRemoved);
    connect(ui->agentStateWidget, &librecelik::agent::AgentStateWidget::retryRequested, this,
            [this]() { gateway->refresh(); });
    // The client resolves presence in its own constructor and emits nothing
    // for the state it starts in, so the first render happens here, after the
    // gateway exists — otherwise a reachable agent's version would stay hidden
    // behind a dash until the agent went away.
    updateAboutText();
    onPresenceChanged(gateway->presence());
    onReadersChanged();
}

void LibreCelik::updateAboutText()
{
    // A language change can reach this before the gateway exists, and no agent
    // answers in the guided states — both are the same honest "not known yet",
    // rendered as a dash rather than as a blank the reader has to interpret.
    const QString version = gateway ? gateway->agentVersion() : QString();
    const QString versionOrDash = version.isEmpty() ? QStringLiteral("—") : version;
    ui->aboutLabel->setText(QString("<br><br>") + qtTrId("lc-main-about-librecelik").arg(LIBRECELIK_VERSION) +
                            QString("<br>") + qtTrId("lc-main-about-agent").arg(versionOrDash) + QString("<br>") +
                            qtTrId("lc-main-about-donate"));
}

bool LibreCelik::loadLanguage(const QString& locale)
{
    if (locale.isEmpty())
        return false;
    QApplication::removeTranslator(&translator);
    QApplication::removeTranslator(&qtTranslator);
    if (translator.load(":/i18n/LibreCelik_" + locale)) {
        QApplication::installTranslator(&translator);
        // Load Qt's own translations (for About Qt dialog, standard buttons, etc.).
        // Missing translations for some locales are expected (e.g. sr_RS); the
        // application text is still available via our own .qm file installed
        // above, so a failed Qt-catalog load is not fatal.
        if (!qtTranslator.load("qt_" + locale, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            // Intentionally ignored — see comment above.
        }
        QApplication::installTranslator(&qtTranslator);
        this->locale = locale;
        return true;
    }
    return false;
}

void LibreCelik::retranslateMenuBar()
{
    if (editMenu)
        editMenu->setTitle(qtTrId("lc-menu-edit"));
    settingsAction->setText(qtTrId("lc-menu-settings"));
    helpMenu->setTitle(qtTrId("lc-menu-help"));
    aboutAction->setText(qtTrId("lc-menu-about"));
    aboutQtAction->setText(qtTrId("lc-menu-about-qt"));
#ifdef Q_OS_MACOS
    macosRetranslateAppMenu({.about = qtTrId("lc-menu-about"),
                             .preferences = qtTrId("lc-menu-settings"),
                             .services = qtTrId("lc-menu-services"),
                             .hide = qtTrId("lc-menu-hide"),
                             .hideOthers = qtTrId("lc-menu-hide-others"),
                             .showAll = qtTrId("lc-menu-show-all"),
                             .quit = qtTrId("lc-menu-quit")});
#endif
}

void LibreCelik::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange && uiReady) {
        ui->retranslateUi(this);
        retranslateMenuBar();
        updateAboutText();
        // setupUi's retranslate resets the .ui labels to visible; the guided
        // panel's claim over them is state, not text, so it is re-applied.
        updateEmptyState();
    }
    QMainWindow::changeEvent(event);
}

// ---- presence and roster -------------------------------------------------

void LibreCelik::onPresenceChanged(PresenceState state)
{
    if (state != PresenceState::Ready) {
        // Every page belonged to a roster the agent owned. The gateway has
        // already announced each card's removal (the client clears its whole
        // registry with no per-card event, which is exactly the gap the gateway
        // fills), so the pages are gone by now and only the panel is left.
        ui->agentStateWidget->setState(state, false);
        updateEmptyState();
        return;
    }
    importLegacySettings();
    onReadersChanged();
}

void LibreCelik::importLegacySettings()
{
    QSettings settings(settings::kOrganization, settings::kApplication);

    // Settings tier only: DefaultLevel / DefaultReason / DefaultLocation, which
    // the agent's policy lets an active session write without ceremony. Each
    // item carries its own completion marker, so this is idempotent — and it is
    // called on EVERY transition to Ready rather than once per process, because
    // that is what makes the per-item retry real: an item whose write never
    // reached a vanishing agent stays unmarked and is attempted again the next
    // time the agent is there.
    librecelik::agent::runSettingsTierImport(*gateway, settings);

    // The trust tier (TsaUrls / TslSources) is polkit `auth_self` on every row.
    // Writing it here would raise an authorisation dialog at startup that the
    // human never asked for — so it is not written here at all. What it gets is
    // one passive line saying where the old values can be applied; the Settings
    // dialog prefills them, and the ceremony happens on a Save or not at all.
    if (librecelik::agent::shouldShowTrustImportNotice(settings)) {
        ui->statusbar->show();
        ui->statusbar->showMessage(qtTrId("lc-settings-trust-import-notice"));
        settings.setValue(settings::kConfig1TrustNoticeShown, 1);
    }
}

void LibreCelik::onReadersChanged()
{
    const QList<librecelik::agent::ReaderInfo> readers = gateway->readers();
    // Arrivals: a card already in the roster when this window opened, or one
    // whose reader event reached us before its own cardChanged did.
    for (const librecelik::agent::ReaderInfo& reader : readers) {
        if (reader.hasCard && !reader.cardId.isEmpty() && !activeCards.contains(reader.cardId))
            onCardChanged(reader.cardId);
    }
    ui->agentStateWidget->setState(gateway->presence(), !readers.isEmpty());
    updateEmptyState();
}

void LibreCelik::onCardChanged(const QString& objectId)
{
    if (activeCards.contains(objectId))
        return; // a property change on a card that already has its page
    if (failedReads.contains(objectId))
        return; // its read already failed once; only a re-insertion retries it

    // The client's cardChanged carries either a card id or a reader id; the
    // gateway answers nullptr for everything that is not a live card, which is
    // the only test this window needs.
    CardController* controller = gateway->cardController(objectId);
    if (controller == nullptr)
        return;

    addCardPage(objectId, controller);
}

void LibreCelik::onCardRemoved(const QString& cardId)
{
    // A card that leaves clears its failure memory: pulling and re-inserting
    // it is the one gesture that MUST get a fresh read.
    failedReads.erase(cardId);
    if (!activeCards.contains(cardId))
        return;

    // Announce BEFORE teardown so page-level consumers inside this window's
    // widget tree can act while their widgets are still valid.
    emit cardRemoved(cardId);
    releaseCardPage(cardId);
}

void LibreCelik::updateEmptyState()
{
    if (!gateway)
        return; // a language change delivered before the gateway exists
    const bool guided = gateway->presence() != PresenceState::Ready;
    // Do not claim that no card was detected in any reader while LC cannot
    // reach the component that enumerates them — the guided panel is the only
    // honest copy in that state.
    ui->label_3->setVisible(!guided);
    ui->label_4->setVisible(!guided);
    ui->readerComboBox->setVisible(ui->readerComboBox->count() > 1);
    ui->stackedWidget->setCurrentIndex(!guided && !activeCards.empty() ? 1 : 0);
}

QString LibreCelik::readerNameForCard(const QString& cardId) const
{
    const QList<librecelik::agent::ReaderInfo> readers = gateway->readers();
    for (const librecelik::agent::ReaderInfo& reader : readers) {
        if (reader.cardId == cardId)
            return reader.name.isEmpty() ? reader.id : reader.name;
    }
    return cardId;
}

// ---- per-card pages ------------------------------------------------------

void LibreCelik::addCardPage(const QString& cardId, CardController* controller)
{
    const std::uint32_t caps = controller->capabilityBits();
    const UiState state = LibreSCRS::AgentClient::resolveCardState(caps, controller->preReadAuth(), /*present=*/true,
                                                                   /*identityRead=*/false);

    if (state == UiState::UnknownCard) {
        // The agent matched no driver. That is a definitive verdict, not a
        // transient one — it has already had full APDU access to the card —
        // so there is nothing to retry and no page to build.
        ui->statusbar->show();
        ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card-with-atr").arg(atrSnippet(controller->atrHex())));
        return;
    }

    if (state == UiState::Error) {
        // A driver matched, but its capabilities create no user surface at all
        // (ancillary-only). There is nothing to render and nothing to retry.
        ui->statusbar->show();
        ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card"));
        return;
    }

    // A readable card is being added — clear any previous unsupported notice.
    ui->statusbar->clearMessage();

    // PreAuthRequired: startRead() below makes the AGENT prompt for the CAN or
    // MRZ (the holder's choice and every retry live agent-side); LC only says
    // where to look. No secret is ever collected in this process.
    QWidget* spinner = makeSpinnerPage(
        state == UiState::PreAuthRequired ? qtTrId("lc-agent-awaiting-preauth") : qtTrId("lc-reading-card"), this);
    const int pageIndex = ui->readerStackedWidget->addWidget(spinner);
    ui->readerComboBox->addItem(readerNameForCard(cardId));
    ui->readerComboBox->setCurrentIndex(pageIndex);
    activeCards[cardId] = spinner;
    cardState[cardId] = {};
    updateEmptyState();

    connect(controller, &CardController::groupReady, this,
            [this, cardId](const FieldGroup& group) { onGroupReady(cardId, group); });
    connect(controller, &CardController::identityReady, this,
            [this, cardId](const QList<FieldGroup>& groups) { onIdentityReady(cardId, groups); });
    connect(controller, &CardController::cardTypeResolved, this,
            [this, cardId](const QString&) { onCardTypeResolved(cardId); });
    connect(controller, &CardController::certificatesReady, this, [this, cardId](const QList<CertificateInfo>& certs) {
        auto entry = cardState.find(cardId);
        if (entry == cardState.end())
            return;
        entry->second.pendingCertificates = certs;
        applyPendingPki(cardId);
    });
    connect(controller, &CardController::tokenInfoReady, this, [this, cardId](const FieldGroup& tokenGroup) {
        auto entry = cardState.find(cardId);
        if (entry == cardState.end())
            return;
        entry->second.pendingTokenInfo = tokenGroup;
        applyPendingPki(cardId);
    });
    connect(controller, &CardController::credentialsReady, this, [this, cardId](const CredentialList& credentials) {
        auto entry = cardState.find(cardId);
        if (entry == cardState.end())
            return;
        entry->second.pendingCredentials = credentials;
        applyPendingPki(cardId);
    });
    connect(controller, &CardController::errorOccurred, this, [this, cardId](const QString& message) {
        const auto page = activeCards.find(cardId);
        if (page == activeCards.end())
            return; // a stale terminal for a card whose page is already gone
        ui->statusbar->show();
        ui->statusbar->showMessage(message);
        // A failure while the page is still the spinner means the read never
        // produced anything to show. Leaving the spinner turning would be a
        // lie; the page goes and the window falls back to its empty state.
        // The card is remembered as failed so the next roster event does not
        // re-add it and re-run the read that just failed — a reader property
        // change must not turn one failure into a retry storm against a card.
        if (isSpinner(page->second)) {
            failedReads.insert(cardId);
            releaseCardPage(cardId);
        }
    });

    // A card the agent reports no identity data for is REFUSED the identity
    // read (`UnsupportedOnThisCard`), so asking would buy an error line for a
    // model that was never going to exist. Such a card is a PKI surface and
    // nothing else: it goes straight to where a finished read would have left
    // it, with an empty model.
    if (LibreSCRS::AgentClient::has(caps, Cap::IdentityData))
        controller->startRead();
    else
        onIdentityReady(cardId, {});

    // Feature-gated verbs, decided by the ONE Task-8 helper so the decision the
    // CI test drives is the decision production makes.
    const librecelik::agent::OptionalSections sections =
        librecelik::agent::requestOptionalSections(*gateway, *controller);
    if (auto entry = cardState.find(cardId); entry != cardState.end()) {
        entry->second.tokenInfoAllowed = sections.tokenInfo;
        entry->second.credentialsAllowed = sections.credentials;
    }
}

void LibreCelik::releaseCardPage(const QString& cardId)
{
    const auto page = activeCards.find(cardId);
    if (page == activeCards.end())
        return;

    // Teardown-cancel: a page that is going away must not leave its card busy.
    // On the removal path the gateway has already released the controller (and
    // the client sweeps a vanished card's operations with it), so this reaches
    // a controller only on the paths where one still exists — a navigated-away
    // read, or a read that failed before it could build a page.
    if (auto* controller = gateway->cardController(cardId))
        controller->cancel();

    QWidget* widget = page->second;
    if (widget) {
        const int index = ui->readerStackedWidget->indexOf(widget);
        if (index >= 0)
            ui->readerComboBox->removeItem(index);
        ui->readerStackedWidget->removeWidget(widget);
        widget->deleteLater();
    }
    activeCards.erase(page);
    cardState.erase(cardId);

    if (activeCards.empty())
        ui->statusbar->clearMessage();
    updateEmptyState();
}

void LibreCelik::replaceCardWidget(const QString& cardId, QWidget* newWidget)
{
    const auto page = activeCards.find(cardId);
    if (page == activeCards.end()) {
        newWidget->deleteLater();
        return;
    }
    QWidget* oldWidget = page->second;
    const int index = ui->readerStackedWidget->indexOf(oldWidget);
    ui->readerStackedWidget->removeWidget(oldWidget);
    oldWidget->deleteLater();
    ui->readerStackedWidget->insertWidget(index, newWidget);
    ui->readerStackedWidget->setCurrentIndex(index);
    page->second = newWidget;
}

CardWidgetPlugin* LibreCelik::pluginFor(const QString& cardId) const
{
    CardController* controller = gateway->cardController(cardId);
    if (controller == nullptr)
        return nullptr;

    // The wait-vs-fallback decision lives in the tested helper
    // (plugintyperesolution.h): resolved type → its plugin; unresolved →
    // generic token page UNLESS the agent advertises "card-type" AND the card
    // can still resolve (IdentityData present — resolution rides only on a
    // completed identity/photo read, so without the bit waiting is provably
    // unbounded); empty → wait (cardTypeResolved replays what streamed) or
    // nothing to render at all.
    const QString key = librecelik::agent::effectiveGuiPluginKey(gateway->hasFeature(kCardTypeFeature),
                                                                 controller->capabilityBits(), controller->cardType());
    return key.isEmpty() ? nullptr : guiPluginRegistry.findByCardType(key);
}

QWidget* LibreCelik::pluginWidgetOf(QWidget* page) const
{
    auto* scrollArea = qobject_cast<QScrollArea*>(page);
    if (scrollArea == nullptr || scrollArea->widget() == nullptr)
        return nullptr;
    const auto children = scrollArea->widget()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    return children.isEmpty() ? nullptr : children.first();
}

void LibreCelik::onGroupReady(const QString& cardId, const FieldGroup& group)
{
    if (group.key == QLatin1StringView("error"))
        return;

    const auto page = activeCards.find(cardId);
    const auto state = cardState.find(cardId);
    if (page == activeCards.end() || state == cardState.end())
        return;

    CardWidgetPlugin* plugin = pluginFor(cardId);
    if (plugin == nullptr) {
        // Multi-candidate card: the agent has not named the driver yet. Hold
        // the group; cardTypeResolved replays it against the right plugin.
        state->second.bufferedGroups.append(group);
        return;
    }

    if (isSpinner(page->second)) {
        QWidget* emptyWidget = plugin->createEmptyWidget(this);
        if (emptyWidget == nullptr)
            return; // this plugin does not stream — the full model lands at identityReady
        replaceCardWidget(cardId, makeCardPage(emptyWidget, this));
    }

    if (QWidget* pluginWidget = pluginWidgetOf(page->second))
        plugin->addGroup(group, pluginWidget);
}

void LibreCelik::onCardTypeResolved(const QString& cardId)
{
    const auto state = cardState.find(cardId);
    if (state == cardState.end() || state->second.bufferedGroups.isEmpty())
        return;

    // Replay in arrival order through the ordinary path, which now resolves a
    // plugin. Move first: onGroupReady re-buffers on a still-unresolved type,
    // and re-entering it over the list being drained would not terminate.
    const QList<FieldGroup> buffered = std::exchange(state->second.bufferedGroups, {});
    for (const FieldGroup& group : buffered)
        onGroupReady(cardId, group);
}

void LibreCelik::onIdentityReady(const QString& cardId, const QList<FieldGroup>& groups)
{
    const auto page = activeCards.find(cardId);
    if (page == activeCards.end())
        return;

    CardWidgetPlugin* plugin = pluginFor(cardId);
    if (plugin == nullptr) {
        // No GUI plugin for the card type the agent resolved: the read
        // succeeded but this build cannot render it.
        ui->statusbar->show();
        ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card"));
        return;
    }

    const bool visible = hasVisibleData(groups);

    // identityReady carries the AUTHORITATIVE final model, and the streamed
    // page only ever saw what happened to stream: a recovered (instant) read
    // streams nothing but the merged photo group, and the verification group
    // rides the final model alone. Rebuild from the model unconditionally
    // rather than patching the streamed page group by group.
    replaceCardWidget(cardId, makeCardPage(plugin->createWidget(groups, this), this));

    QWidget* pluginWidget = pluginWidgetOf(page->second);
    if (pluginWidget == nullptr)
        return;

    if (!visible)
        plugin->showNoDataMessage(pluginWidget);
    else if (plugin->supportsPrinting())
        plugin->enablePrintButton(pluginWidget);

    auto* scrollArea = qobject_cast<QScrollArea*>(page->second);
    QWidget* container = scrollArea ? scrollArea->widget() : nullptr;
    CardController* controller = gateway->cardController(cardId);
    if (container != nullptr && controller != nullptr &&
        LibreSCRS::AgentClient::has(controller->capabilityBits(), Cap::Pki)) {
        attachPkiSection(cardId, container, /*collapsed=*/visible);
        controller->requestCertificates();
    }
}

void LibreCelik::attachPkiSection(const QString& cardId, QWidget* container, bool collapsed)
{
    auto* containerLayout = qobject_cast<QVBoxLayout*>(container->layout());
    if (containerLayout == nullptr || container->findChild<TokenSection*>() != nullptr)
        return;

    CardWidgetPlugin* plugin = pluginFor(cardId);
    QWidget* pkiWidget = plugin ? plugin->createPKIWidget(container) : nullptr;
    if (pkiWidget == nullptr) {
        auto* section = new TokenSection(container);
        section->setHeaderColor(QColor(230, 135, 60));
        section->setHeaderHeight(56);
        section->setExpanded(!collapsed);
        section->setCardId(cardId);

        // The certificate viewer renders the record it is handed; the gateway
        // is what serves the raw bytes behind its export action, and the
        // certificate lives in the reader the card sits in.
        //
        // Parented to the section, not to the window: the section is destroyed
        // with its card's page, and a modal child of the window would outlive
        // the data it renders.
        //
        // Queued because the emitter runs inside a stack-allocated QMenu's
        // exec() loop: a direct connection would open the dialog's nested loop
        // while that menu is still on the call stack, and a card pulled at that
        // moment reaps the menu from the section's child list — a free() on a
        // stack pointer.
        connect(
            section, &TokenSection::certificateDetailsRequested, section,
            [this, section, cardId](const CertificateInfo& cert) {
                CardController* controller = gateway->cardController(cardId);
                CertificateViewerDlg dlg(cert, gateway.get(), section, controller ? controller->readerId() : QString());
                dlg.exec();
            },
            Qt::QueuedConnection);
#ifdef LIBRECELIK_SIGNING_ENABLED
        // No session, no plugin, no signing service to juggle: the agent owns
        // the card, and the wizard asks the gateway for everything it needs.
        // Closing on card removal is the wizard's own gateway subscription.
        //
        // Queued for the same reason as the viewer above: the request is
        // emitted from inside a stack-allocated QMenu's exec() loop.
        connect(
            section, &TokenSection::signRequested, this,
            [this](const CertificateInfo& cert, const QString& signCardId) {
                SigningWizard wizard(cert, signCardId, gateway.get(), this);
                wizard.exec();
            },
            Qt::QueuedConnection);
#endif
        // The credential dialog is a verb launcher over the agent's prompter:
        // it collects no secret, so this window carries none either. The
        // controller's mutation results and phase stream go straight to it,
        // and the re-listing after every mutation comes from the controller
        // itself. Closing on card removal is the dialog's own gateway
        // subscription.
        //
        // Queued for the same reason as the viewer and the wizard above: the
        // request is emitted from inside a stack-allocated QMenu's exec() loop,
        // and a direct connection would open a nested modal loop on top of it.
        connect(
            section, &TokenSection::changePinRequested, this,
            [this, cardId](const LibreSCRS::AgentClient::CredentialRecord& credential) {
                auto* ctrl = gateway->cardController(cardId);
                if (!ctrl)
                    return;
                ChangePinDlg dlg(credential, gateway.get(), cardId, this);
                connect(&dlg, &ChangePinDlg::verbRequested, ctrl, &librecelik::agent::CardController::managePin);
                connect(ctrl, &librecelik::agent::CardController::pinResultReady, &dlg, &ChangePinDlg::onPinResult);
                connect(ctrl, &librecelik::agent::CardController::pinPhaseChanged, &dlg, &ChangePinDlg::onPhase);
                dlg.exec();
            },
            Qt::QueuedConnection);
        pkiWidget = section;
    }
    containerLayout->addWidget(pkiWidget);
    applyPendingPki(cardId);
}

void LibreCelik::applyPendingPki(const QString& cardId)
{
    const auto page = activeCards.find(cardId);
    const auto state = cardState.find(cardId);
    if (page == activeCards.end() || state == cardState.end())
        return;

    auto* section = page->second->findChild<TokenSection*>();
    if (section == nullptr)
        return; // the section is built when the identity read completes

    // The Task-8 verdicts gate the RENDER, not merely the dispatch: the token
    // block and the credential rows appear exactly when their verb was issued,
    // so visibility cannot drift away from what LC actually asked the agent
    // for. Certificates carry no such gate — reading them is a core verb.
    if (state->second.tokenInfoAllowed && state->second.pendingTokenInfo)
        section->setTokenInfo(*std::exchange(state->second.pendingTokenInfo, std::nullopt));
    if (state->second.pendingCertificates)
        section->setCertificates(*std::exchange(state->second.pendingCertificates, std::nullopt));
    if (state->second.credentialsAllowed && state->second.pendingCredentials)
        section->setCredentials(*std::exchange(state->second.pendingCredentials, std::nullopt));
}

void LibreCelik::openSettings()
{
    SettingsDialog dlg(gateway.get(), this);
    connect(&dlg, &SettingsDialog::languageChanged, this,
            [this](const QString& newLocale) { loadLanguage(newLocale); });
    dlg.exec();
}

void LibreCelik::showAboutDialog()
{
    AboutDialog dlg(gateway ? gateway->agentVersion() : QString(), this);
    dlg.exec();
}

LibreCelik::~LibreCelik()
{
    delete ui;
}
