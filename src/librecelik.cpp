// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "librecelik.h"
#include "aboutdialog.h"
#include "agent/cardretrypage.h"
#include "agent/errortext.h" // readFailureAction
#include "agent/agentstatewidget.h"
#include "agent/cardcontroller.h"
#include "agent/cardstatuspage.h"
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
#include "utils/spinnerpage.h"

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
#include <QLibraryInfo>
#include <QLocale>
#include <QMenu>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

using librecelik::agent::AgentGateway;
using librecelik::agent::CardController;
using librecelik::agent::PresenceState;
using librecelik::utils::isSpinner;
using librecelik::utils::makeSpinnerPage;
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
    readerPages = new ReaderPages(ui->readerComboBox, ui->readerStackedWidget, this);
    // Reader selection: cancel the read of the card being navigated away from.
    // A page the user has left must not keep its card busy — this is what the
    // middleware path's per-reader stop source used to do. ReaderPages raises
    // this ONLY for a real selection change: adding and removing a page repair
    // the selector themselves, and used to reach here with a stale index and
    // cancel a bystander's read.
    connect(readerPages, &ReaderPages::leftCard, this, [this](const QString& cardId) {
        if (auto* controller = gateway->cardController(cardId))
            controller->cancel();
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
    // FULL, for the same reason the About dialog uses it: this banner is the
    // version a user reads off the screen, and the numeric triple alone would
    // present a between-tags build as the release it is descended from.
    ui->aboutLabel->setText(QString("<br><br>") + qtTrId("lc-main-about-librecelik").arg(LIBRECELIK_VERSION_FULL) +
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
        if (reader.hasCard && !reader.cardId.isEmpty() && !readerPages->contains(reader.cardId))
            onCardChanged(reader.cardId);
    }
    ui->agentStateWidget->setState(gateway->presence(), !readers.isEmpty());
    updateEmptyState();
}

void LibreCelik::onCardChanged(const QString& objectId)
{
    if (readerPages->contains(objectId))
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
    if (!readerPages->contains(cardId))
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
    ui->stackedWidget->setCurrentIndex(!guided && !readerPages->isEmpty() ? 1 : 0);
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

    if (librecelik::agent::hasNoCardSurface(state)) {
        // Nothing to read and nothing to retry: the agent matched no driver
        // (a definitive verdict — it has already had full APDU access to the
        // card), or the driver it matched is ancillary-only.
        //
        // The card still gets a PAGE rather than a line on the window's single
        // status bar. The bar is global: the next readable card's arrival
        // cleared the notice, and two readers holding two unreadable cards
        // could only ever show one of them — while neither card appeared in
        // the reader selector at all. The page carries the reader's name
        // itself, because the selector is hidden while there is only one page.
        //
        // The verdict is per CARD, in the reader that holds it — never a
        // window-wide "nothing readable anywhere" state: two readers can hold
        // two different cards and each has to speak for itself.
        //
        // Nothing beyond this point applies to such a card: no read is
        // dispatched, so no controller signal can arrive, and the page stands
        // until the card leaves.
        registerCardPage(cardId, new librecelik::agent::CardStatusPage(state, readerNameForCard(cardId),
                                                                       controller->atrHex(), this));
        return;
    }

    // The status bar carries transient notices (trust-list import results,
    // another card's error line); a fresh readable card starts from a clean
    // line so its own outcome is not read against a stale message.
    ui->statusbar->clearMessage();

    // PreAuthRequired: startRead() below makes the AGENT prompt for the CAN or
    // MRZ (the holder's choice and every retry live agent-side); LC only says
    // where to look. No secret is ever collected in this process.
    QWidget* spinner = makeSpinnerPage(
        state == UiState::PreAuthRequired ? qtTrId("lc-agent-awaiting-preauth") : qtTrId("lc-reading-card"), this);
    registerCardPage(cardId, spinner);

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
    connect(controller, &CardController::errorOccurred, this,
            [this, cardId, controller](const QString& message, LibreSCRS::AgentClient::ErrorCode code) {
                if (readerPages->page(cardId) == nullptr)
                    return; // a stale terminal for a card whose page is already gone
                ui->statusbar->show();
                ui->statusbar->showMessage(message);
                // The three-way decision lives in the tested helper, not here:
                // no test binary links this file, so a rule written inline is a
                // rule no gate can reach (the requestOptionalSections pattern).
                switch (librecelik::agent::readFailureAction(isSpinner(readerPages->page(cardId)), code)) {
                case librecelik::agent::ReadFailureAction::LeaveAlone:
                    break;
                case librecelik::agent::ReadFailureAction::LatchAndDrop:
                    // Nothing was rendered and repeating cannot help. The page
                    // goes, and the card is remembered as failed so the next
                    // roster event does not re-run the read that just failed —
                    // a reader property change must not turn one failure into a
                    // retry storm against a card.
                    failedReads.insert(cardId);
                    releaseCardPage(cardId);
                    break;
                case librecelik::agent::ReadFailureAction::OfferRetry:
                    // Nothing was rendered, but the holder can clear this one.
                    // NOT latched — that would strand them — and no longer left
                    // spinning either: the spinner's text points at a system
                    // window that is already gone.
                    offerCardReadRetry(cardId, message);
                    break;
                }
            });

    // A card the agent reports no identity data for is REFUSED the identity
    // read (`UnsupportedOnThisCard`), so asking would buy an error line for a
    // model that was never going to exist. Such a card is a PKI surface and
    // nothing else: it goes straight to where a finished read would have left
    // it, with an empty model.
    if (LibreSCRS::AgentClient::has(caps, Cap::IdentityData)) {
        cardState[cardId].identityReadStarted = true;
        controller->startRead();
    } else {
        onIdentityReady(cardId, {});
    }
}

void LibreCelik::offerCardReadRetry(const QString& cardId, const QString& message)
{
    auto* page = new librecelik::agent::CardRetryPage(message, readerNameForCard(cardId), this);
    connect(page, &librecelik::agent::CardRetryPage::retryRequested, this, [this, cardId] {
        // Re-resolved rather than captured: the controller is owned by the
        // gateway, and the removal path releases it before this page goes. It
        // cannot dangle today because both happen in one synchronous chain, but
        // holding a raw reference makes that ordering load-bearing, and every
        // other card verb in this file re-resolves.
        CardController* controller = gateway->cardController(cardId);
        if (controller == nullptr)
            return; // the card left while its retry page was on screen

        // Drop what the FAILED attempt streamed. Groups are buffered whenever
        // the card's type has not resolved yet — which is exactly the
        // multi-candidate card that reaches a pre-auth failure — and the buffer
        // is drained only by cardTypeResolved, which a failed read never
        // reaches. Retrying without this appends the second attempt's groups to
        // the first attempt's, and the eventual replay renders every section
        // twice. replaceCardWidget preserves this card's state map, which is the
        // point; it preserves the stale buffer with it.
        if (auto entry = cardState.find(cardId); entry != cardState.end())
            entry->second.bufferedGroups.clear();

        // Spinner FIRST, then the read. The controller may answer synchronously,
        // and a second failure arriving before the spinner is up would find the
        // retry page instead — decided as LeaveAlone, leaving a stale reason on
        // screen for a read that has since failed again.
        replaceCardWidget(cardId, makeSpinnerPage(qtTrId("lc-reading-card"), this));
        controller->startRead();
    });
    // replaceCardWidget, never registerCardPage: the latter resets this card's
    // state map, which is where the identity-success path records whether this
    // card may be asked for token info and credentials. Losing that silently
    // drops both sections until the card is re-seated — and since that record is
    // now written when the read SUCCEEDS rather than when the card appears, a
    // reset here also loses it for a card whose retry is still in flight.
    replaceCardWidget(cardId, page);
}

void LibreCelik::registerCardPage(const QString& cardId, QWidget* page)
{
    readerPages->add(cardId, readerNameForCard(cardId), page);
    cardState[cardId] = {};
    updateEmptyState();
}

void LibreCelik::releaseCardPage(const QString& cardId)
{
    if (!readerPages->contains(cardId))
        return;

    // Teardown-cancel: a page that is going away must not leave its card busy.
    // On the removal path the gateway has already released the controller (and
    // the client sweeps a vanished card's operations with it), so this reaches
    // a controller only on the paths where one still exists — a navigated-away
    // read, or a read that failed before it could build a page.
    if (auto* controller = gateway->cardController(cardId))
        controller->cancel();

    readerPages->remove(cardId);
    cardState.erase(cardId);

    if (readerPages->isEmpty())
        ui->statusbar->clearMessage();
    updateEmptyState();
}

void LibreCelik::replaceCardWidget(const QString& cardId, QWidget* newWidget)
{
    readerPages->replace(cardId, newWidget);
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

    QWidget* pageWidget = readerPages->page(cardId);
    const auto state = cardState.find(cardId);
    if (pageWidget == nullptr || state == cardState.end())
        return;

    CardWidgetPlugin* plugin = pluginFor(cardId);
    if (plugin == nullptr) {
        // Multi-candidate card: the agent has not named the driver yet. Hold
        // the group; cardTypeResolved replays it against the right plugin.
        state->second.bufferedGroups.append(group);
        return;
    }

    if (isSpinner(pageWidget)) {
        QWidget* emptyWidget = plugin->createEmptyWidget(this);
        if (emptyWidget == nullptr)
            return; // this plugin does not stream — the full model lands at identityReady
        replaceCardWidget(cardId, makeCardPage(emptyWidget, this));
    }

    // replaceCardWidget above may have swapped the page, so re-read it rather
    // than reusing the pointer captured at entry.
    if (QWidget* pluginWidget = pluginWidgetOf(readerPages->page(cardId)))
        plugin->addGroup(group, pluginWidget);
}

void LibreCelik::onCardTypeResolved(const QString& cardId)
{
    const auto state = cardState.find(cardId);
    if (state == cardState.end())
        return;

    // Replay in arrival order through the ordinary path, which now resolves a
    // plugin. Move first: onGroupReady re-buffers on a still-unresolved type,
    // and re-entering it over the list being drained would not terminate.
    const QList<FieldGroup> buffered = std::exchange(state->second.bufferedGroups, {});
    for (const FieldGroup& group : buffered)
        onGroupReady(cardId, group);

    // The provisional side of the generic-token fallback: a card that could
    // not resolve at add time never got its identity read, and the resolved
    // card may carry the IdentityData bit the candidate intersection dropped.
    // The read ordered here is what re-plugins the page — identityReady
    // rebuilds it from the final model under the now-resolved family plugin.
    CardController* controller = gateway->cardController(cardId);
    if (controller != nullptr &&
        librecelik::agent::lateResolutionStartsRead(state->second.identityReadStarted, controller->capabilityBits())) {
        state->second.identityReadStarted = true;
        controller->startRead();
    }
}

void LibreCelik::onIdentityReady(const QString& cardId, const QList<FieldGroup>& groups)
{
    QWidget* pageWidget = readerPages->page(cardId);
    if (pageWidget == nullptr)
        return;

    // Feature-gated verbs, decided by the ONE helper so the decision the CI test
    // drives is the decision production makes.
    //
    // Dispatched HERE, on the identity read's success, not when the card is
    // added: token info and credentials ride the same authenticated channel the
    // identity read establishes, so asking for them first buys a refusal apiece
    // when that read cannot authenticate -- and on a card that needs an access
    // number, one entry window per verb, serialised, each with its own full
    // deadline. The holder saw three windows two minutes apart for one insertion.
    if (CardController* optionalsController = gateway->cardController(cardId); optionalsController != nullptr) {
        const librecelik::agent::OptionalSections sections =
            librecelik::agent::requestOptionalSections(*gateway, *optionalsController);
        if (auto entry = cardState.find(cardId); entry != cardState.end()) {
            entry->second.tokenInfoAllowed = sections.tokenInfo;
            entry->second.credentialsAllowed = sections.credentials;
        }
    }

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

    // The page was just replaced; re-read it instead of the entry pointer.
    QWidget* pluginWidget = pluginWidgetOf(readerPages->page(cardId));
    if (pluginWidget == nullptr)
        return;

    if (!visible)
        plugin->showNoDataMessage(pluginWidget);
    else if (plugin->supportsPrinting())
        plugin->enablePrintButton(pluginWidget);

    auto* scrollArea = qobject_cast<QScrollArea*>(readerPages->page(cardId));
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
    QWidget* pageWidget = readerPages->page(cardId);
    const auto state = cardState.find(cardId);
    if (pageWidget == nullptr || state == cardState.end())
        return;

    auto* section = pageWidget->findChild<TokenSection*>();
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
