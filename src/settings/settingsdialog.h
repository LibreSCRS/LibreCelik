// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/AgentClient/SyncError.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

class QComboBox;
class QFrame;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;

namespace librecelik::agent {
class AgentGateway;
}

/// @brief Application preferences, split by who owns them.
///
/// The General tab (language) and the default output folder are this process's
/// own and stay in QSettings. Everything that changes how a signature is MADE
/// or what is TRUSTED belongs to the agent and is read from, and written back
/// to, its Config1 surface — the dialog never keeps a second copy of that
/// state, and never invents defaults for it (a per-tab "restore defaults"
/// hands the keys to `Config1.Reset` and lets the agent's own defaults win).
///
/// @note The agent's configuration is shared: another client can change it
///       while this dialog is open. The dialog subscribes to
///       `AgentGateway::configChanged` and re-reads the snapshot when it fires,
///       so the visible state follows the agent — and a Save writes only what
///       the human actually changed, on a LAST-WRITE-WINS basis. There is no
///       merge and no conflict prompt: the agent holds one value per key, and
///       the last writer of a key owns it.
///
/// @note The trust-tier keys (`TsaUrls`, `TslSources`) are polkit-guarded
///       `auth_self` on every write. The authorisation dialog therefore appears
///       on a user-clicked Save, in context — never at startup. A refusal is
///       rendered once on the status label and the Save stops there; the dialog
///       never re-issues the write on its own.
///
/// @note Installing country-signing anchors is the same tier and the same
///       posture, only stronger: an import does not name a place anchors may
///       come from, it INSTALLS the anchors travel documents are accepted or
///       refused against. It happens on a click, its refusal is rendered once,
///       and it is never retried. What is already installed IS readable — the
///       agent serves it as a read-only property — so the Trust tab accounts
///       for it on open, without an import having happened. See `cscaState`.
///
/// @note The Trust tab NAMES where a master list is downloaded from, and says
///       that this application never downloads one itself. Both halves are
///       needed: the import is unusable without a file, and the download is
///       gated behind terms a person has to accept, so the absence of an
///       automatic fetch is a decision rather than a missing feature. Exactly
///       ONE address is named — the publisher's own public portal. Other
///       issuers publish lists too, but an address that turns out to be wrong
///       costs a reader more than saying nothing would.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    /// @param gateway  Agent gateway — NOT owned; it outlives this modal.
    ///                 A null gateway (or any presence other than Ready)
    ///                 renders the operation-backed tabs disabled.
    explicit SettingsDialog(librecelik::agent::AgentGateway* gateway, QWidget* parent = nullptr);

    /// Install country-signing (CSCA) trust anchors from an OPEN master-list
    /// descriptor, then say what came back.
    ///
    /// @param masterListFd BORROWED: the agent duplicates it for the wire and
    ///        this call never closes yours. Its read shares one open file
    ///        description with yours, so YOUR OFFSET MOVES — rewind before
    ///        reading the same descriptor again.
    ///
    /// Does nothing at all without a Ready agent, which is the same rule the
    /// rest of the operation-backed tabs follow.
    void importMasterList(int masterListFd);

    /// Open @p path and hand the resulting DESCRIPTOR to `importMasterList()`
    /// — never the name. The agent is a separate, possibly sandboxed process:
    /// a name it would have to re-open is a name it may not be able to open,
    /// and it would resolve a second time what this process already resolved
    /// once. The descriptor is closed on the way out, whatever the answer was.
    ///
    /// A file this process cannot open never becomes a round-trip: there is
    /// nothing to hand over, and dialling anyway would spend an authorization
    /// ceremony on a file that was never read.
    void importMasterListFile(const QString& path);

signals:
    void languageChanged(const QString& locale);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void loadSettings();
    /// Re-read the agent's snapshot into every operation-backed control.
    void loadConfig();
    /// Enable/disable the operation-backed tabs from the current presence.
    void applyPresence();
    /// Render the last refusal (if any). Called from retranslateUi() so the
    /// line follows a language switch instead of freezing at first render.
    void renderStatus();
    /// Persist the file-backed half and write the changed Config1 keys.
    /// @return false when a write was refused (the dialog then stays open).
    bool saveSettings();
    /// `Config1.Reset` for every key of one tab, then a fresh snapshot read.
    void restoreDefaults(const QStringList& keys);
    [[nodiscard]] QStringList tsaUrlsFromList() const;
    [[nodiscard]] QVariantList tslSourcesFromList() const;
    void populateTsaList();
    void onTsaAddRequested();
    void populateTlList();
    void onTlAddRequested();
    /// Choose a master list, then hand its descriptor over.
    void onCscaImportRequested();
    /// Redraw the anchor account and the last import's outcome. Called from
    /// retranslateUi() so both follow a language switch instead of freezing.
    void renderCscaState();

    /// NOT owned — the window that opened this dialog owns it.
    librecelik::agent::AgentGateway* gateway = nullptr;
    /// The agent's snapshot as last read. The comparison base a Save uses to
    /// decide which keys the human actually changed.
    QVariantMap config;
    /// Legacy trust-tier values (TsaUrls/TslSources), read once from QSettings
    /// at construction. DISPLAY ONLY: they seed the lists where the agent
    /// carries nothing yet, and they are never part of the comparison base — a
    /// Save writes them exactly as it would write anything the human typed, so
    /// the polkit ceremony stays on the click and never on startup.
    QVariantMap trustPrefill;
    /// The refusal the last Save (or restore) collected, if any.
    std::optional<LibreSCRS::AgentClient::SyncError> lastRefusal;

    /// What the last master-list import in this dialog's lifetime did. `None`
    /// means there has not been one, and nothing is said about it.
    enum class CscaImportOutcome { None, Installed, Replayed, Unauthorized, Refused, Unreadable };
    CscaImportOutcome cscaOutcome = CscaImportOutcome::None;
    /// What the agent holds in country-signing anchors, in the shape
    /// `configSnapshot()["CscaAnchorState"]` carries it. Read on open and
    /// refreshed with every snapshot; an accepted import replaces it with the
    /// state that import answered with, spelled into the same shape.
    ///
    /// A MAP rather than the client's `CscaAnchorState` value struct, and that
    /// is the whole point: an optional member the agent did not send is an
    /// ABSENT KEY, while the struct would zero it. Only the map can tell "the
    /// accepted list carried no signing time" from "signed on 1970-01-01".
    ///
    /// EMPTY means nothing has been imported — and it is equally what a client
    /// sees when the agent discarded a stale record because its anchor cache
    /// had been wiped. Those two cannot be told apart from here, and both are
    /// honestly "nothing installed"; a third state this dialog cannot
    /// distinguish would be a claim nobody measured.
    ///
    /// A refused import leaves this exactly as it was: nothing was installed
    /// and nothing already held was given up.
    QVariantMap cscaState;
    /// True while a Save/restore is writing: the `configChanged` each write
    /// announces must not re-read the snapshot into the controls mid-run.
    bool writeInFlight = false;

    QTabWidget* tabs = nullptr;
    QLabel* needsAgentLabel = nullptr;
    QLabel* statusLabel = nullptr;

    // General tab
    QLabel* languageLabel = nullptr;
    QComboBox* languageCombo = nullptr;
    QString originalLocale;

    // Signing tab
    QLabel* defaultLevelLabel = nullptr;
    QLabel* defaultOutputLabel = nullptr;
    QLabel* defaultReasonLabel = nullptr;
    QLabel* defaultLocationLabel = nullptr;
    QLabel* lastTsaLabel = nullptr;
    QLabel* lastTsaValue = nullptr;
    QLabel* tsaServersLabel = nullptr;
    QComboBox* defaultLevelCombo = nullptr;
    QLineEdit* defaultOutputFolder = nullptr;
    QLineEdit* defaultReasonEdit = nullptr;
    QLineEdit* defaultLocationEdit = nullptr;
    QPushButton* browseOutputBtn = nullptr;
    QPushButton* signingRestoreDefaultsBtn = nullptr;
    QListWidget* tsaList = nullptr;

    // Trust tab. Two framed sections, one per setting the tab owns: the lists
    // signatures are validated against, and the anchors travel documents are
    // checked against. The heading of each is the group's own title, so neither
    // half can read as a footnote under the other.
    QGroupBox* tlGroup = nullptr;
    QPushButton* trustRestoreDefaultsBtn = nullptr;
    QListWidget* tlList = nullptr;
    QGroupBox* cscaGroup = nullptr;
    /// The anchor frame's upper half: what the agent HOLDS. A reading of the
    /// system, and the rule below it is what keeps it from being read as the
    /// first line of the advice underneath.
    QLabel* cscaSummaryLabel = nullptr;
    QLabel* cscaStatusLabel = nullptr;
    /// The anchor frame's lower half: three sentences carrying three different
    /// kinds of thing, and rendered as three, not as a wall.
    ///
    /// `cscaHelpLabel` is the INSTRUCTION — where a master list is downloaded
    /// from — and it is the one a reader came for, so it keeps the body voice
    /// and carries the live link. The DEFINITION above it and the RATIONALE
    /// below are context: both are set in the application's quiet voice (one
    /// point down, placeholder colour) so the eye can skip them and land on
    /// the address. Reading order still runs definition, instruction,
    /// rationale, because "download one" needs the noun named ahead of it.
    QLabel* cscaWhatLabel = nullptr;
    QLabel* cscaHelpLabel = nullptr;
    QLabel* cscaManualLabel = nullptr;
    QPushButton* cscaImportButton = nullptr;
};
