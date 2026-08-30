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
///       and it is never retried. What is already installed cannot be read
///       back — see `cscaState` for why the Trust tab says so rather than
///       showing a count it never took.
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
    /// The anchor state an ACCEPTED import answered with — the only way this
    /// dialog ever learns one.
    ///
    /// Disengaged means NOT ASKED, never "nothing installed". The client
    /// library does not demarshal `Config1.CscaAnchorState`, so a dialog that
    /// has just opened cannot read what the agent already holds; rendering
    /// zeros there would be a reading nobody took, and a reader would act on
    /// it. A refusal leaves this exactly as it was: nothing was installed and
    /// nothing already held was given up.
    std::optional<LibreSCRS::AgentClient::CscaAnchorState> cscaState;
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

    // Trust tab
    QLabel* tlServersLabel = nullptr;
    QPushButton* trustRestoreDefaultsBtn = nullptr;
    QListWidget* tlList = nullptr;
    QLabel* cscaAnchorsLabel = nullptr;
    QLabel* cscaSummaryLabel = nullptr;
    QLabel* cscaStatusLabel = nullptr;
    QPushButton* cscaImportButton = nullptr;
};
