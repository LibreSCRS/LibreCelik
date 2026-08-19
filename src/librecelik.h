// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "agent/agentgateway.h"
#include "config.h"
#include "plugin/cardwidgetpluginregistry.h"
#include "utils/readerpages.h"

#include <LibreSCRS/AgentClient/CredentialTypes.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QTranslator>

#include <map>
#include <memory>
#include <optional>
#include <set>

class SettingsDialog;

namespace librecelik::agent {
class CardController;
}

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
    /// @brief Emitted for every card that leaves the agent's roster, forwarded
    /// from `AgentGateway::cardRemoved` BEFORE this window tears the card's
    /// page down. Page-level consumers that live inside this window's widget
    /// tree filter by @p cardId and react while the page is still valid.
    ///
    /// The payload is the agent's opaque CARD id, not a reader name: the
    /// gateway keys removal by card, and a reader can outlive the card in it.
    /// Modal hygiene no longer rides this signal — the wizard and the PIN
    /// dialog subscribe to `AgentGateway::cardRemoved` themselves, so their
    /// close-on-removal behaviour lives inside the testable dialogs instead of
    /// in window glue (Tasks 24/27).
    void cardRemoved(QString cardId);

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onPresenceChanged(librecelik::agent::PresenceState state);
    void onReadersChanged();
    void onCardChanged(const QString& objectId);
    void onCardRemoved(const QString& cardId);

private:
    /// Per-card read bookkeeping. `readerPages` maps a card to its page and
    /// keeps the selector in step with the stack; this
    /// maps the same card to what the page still needs and does not have yet —
    /// the F5 section verdicts, the groups that streamed before a plugin could
    /// render them, and the PKI payloads that arrived before the section
    /// existed. Both maps are keyed by card id and live and die together.
    struct CardReadState
    {
        /// The Task-8 feature-gate verdicts, exactly as dispatched: a section
        /// whose verb was never issued must never render, so the same flag
        /// that dispatched the verb gates the apply.
        bool tokenInfoAllowed = false;
        bool credentialsAllowed = false;
        /// Groups that streamed before the card's type — and therefore its GUI
        /// plugin — was resolved. Replayed on `cardTypeResolved`; a
        /// multi-candidate card would otherwise render nothing that streamed
        /// before the agent picked the driver.
        QList<LibreSCRS::AgentClient::FieldGroup> bufferedGroups;
        /// Whether the identity read was dispatched for this card. The add
        /// path skips the read entirely when the card carries no IdentityData
        /// — so a LATE type resolution must order the read itself (see
        /// lateResolutionStartsRead), and must never double-start one that is
        /// already running.
        bool identityReadStarted = false;
        /// PKI payloads that arrived before the token section was built (the
        /// section is created when the identity read completes; the optional
        /// sections are dispatched with the read).
        std::optional<LibreSCRS::AgentClient::FieldGroup> pendingTokenInfo;
        std::optional<QList<LibreSCRS::AgentClient::CertificateInfo>> pendingCertificates;
        std::optional<LibreSCRS::AgentClient::CredentialList> pendingCredentials;
    };

    void addCardPage(const QString& cardId, librecelik::agent::CardController* controller);
    /// Give @p page the card's slot in the reader stack, the reader selector
    /// and both per-card maps. Every card that gets a page goes through here —
    /// the readable card's spinner and the unreadable card's status page
    /// alike — so a card can never end up in one of the four and not the rest.
    void registerCardPage(const QString& cardId, QWidget* page);
    void releaseCardPage(const QString& cardId);
    void replaceCardWidget(const QString& cardId, QWidget* newWidget);
    void onGroupReady(const QString& cardId, const LibreSCRS::AgentClient::FieldGroup& group);
    void onIdentityReady(const QString& cardId, const QList<LibreSCRS::AgentClient::FieldGroup>& groups);
    void onCardTypeResolved(const QString& cardId);
    void attachPkiSection(const QString& cardId, QWidget* container, bool collapsed);
    void applyPendingPki(const QString& cardId);
    /// The GUI plugin for @p cardId's currently resolved card type, or nullptr
    /// while the agent has resolved none (or none is installed for it).
    [[nodiscard]] CardWidgetPlugin* pluginFor(const QString& cardId) const;
    /// The page's plugin widget — the first direct child of the scroll area's
    /// container — or nullptr when the page is not a built card page.
    [[nodiscard]] QWidget* pluginWidgetOf(QWidget* page) const;
    /// Page 0 vs page 1, the reader selector's visibility, and the guided
    /// panel's claim over the window's own "insert a card" copy.
    void updateEmptyState();
    [[nodiscard]] QString readerNameForCard(const QString& cardId) const;

    /// Carry the legacy QSettings preferences over to the agent's Config1.
    /// Settings tier ONLY, per item, idempotent; the trust tier gets a passive
    /// one-time notice and is never written from here (polkit ceremony belongs
    /// on a user click, not on startup). Runs on every transition to Ready.
    void importLegacySettings();

    bool loadLanguage(const QString& locale);
    void updateAboutText();
    void retranslateMenuBar();
    void openSettings();
    void showAboutDialog();

private:
    Ui::LibreCelik* ui;

    /// The only card I/O seam this window has. Constructed as the live gateway;
    /// every card verb, every roster fact and every presence verdict comes
    /// through it, and nothing in this window talks to a card directly.
    std::unique_ptr<librecelik::agent::AgentGateway> gateway;
    CardWidgetPluginRegistry guiPluginRegistry;

    /// One page per card in the agent's roster, keyed by the agent's opaque
    /// card id. The page widget is whatever the card currently renders as: the
    /// spinner while it reads, the plugin's scroll area once it has data.
    /// Reader selector and page stack, and the card-to-page mapping between
    /// them. Owned here, exercised on its own — see utils/readerpages.h.
    ReaderPages* readerPages = nullptr;
    std::map<QString, CardReadState> cardState;
    /// Cards whose read failed before it could build a page. They keep their
    /// place in the agent's roster, so without this memory the next reader
    /// event would re-add them and re-run the read that just failed. Cleared
    /// when the card actually leaves — a re-insertion is a fresh read.
    std::set<QString> failedReads;

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
};
