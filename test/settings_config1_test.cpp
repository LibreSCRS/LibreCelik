// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Offscreen coverage for the settings dialog over the agent's Config1.
///
/// The operation-affecting preferences are the AGENT's, not a file this
/// process owns: the level a signature defaults to, the timestamp authorities
/// it may use, the trusted lists it validates against. So what there is to
/// assert is what the dialog READS from the agent's snapshot, what it WRITES
/// back through `setConfigValue` (in the wire's own spelling, not the combo's),
/// which keys a per-tab "restore defaults" hands to `resetConfigValue`, what it
/// SAYS when a write is refused — and that a refusal is said exactly once,
/// never retried. The file-only cache-directory control is gone, and the
/// operation-backed tabs go dark when no agent is there to keep them.
///
/// Nothing here dials anything: the gateway is the campaign's scripted fake.

#include "settings/settingsdialog.h"

#include "fake_gateway/fakeagentgateway.h"
#include "settings/tlitemdelegate.h"

#include <LibreSCRS/AgentClient/SyncError.h>

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTime>
#include <QTimeZone>
#include <QTranslator>
#include <QVariant>

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>

namespace {

using librecelik::test::agent::FakeAgentGateway;

/// The dialog commits on OK; the button carries an object name precisely so a
/// test can press the same thing a human presses.
void invokeSave(SettingsDialog& dlg)
{
    auto* ok = dlg.findChild<QPushButton*>(QStringLiteral("okButton"));
    ASSERT_NE(ok, nullptr);
    ok->click();
}

QString statusLabelText(SettingsDialog& dlg)
{
    auto* label = dlg.findChild<QLabel*>(QStringLiteral("statusLabel"));
    return label != nullptr ? label->text() : QString();
}

/// Replace the trust tab's list content with @p urls, leaving the translated
/// "add" sentinel row where the delegate expects it — exactly the shape
/// `onTlAddRequested()` leaves behind after a human adds a list.
void setTrustTabTslList(SettingsDialog& dlg, const QStringList& urls)
{
    auto* list = dlg.findChild<QListWidget*>(QStringLiteral("tlList"));
    ASSERT_NE(list, nullptr);
    for (int row = list->count() - 1; row >= 0; --row) {
        if (list->item(row)->data(TlItemDelegate::TypeRole).toString() != QStringLiteral("add")) {
            delete list->takeItem(row);
        }
    }
    for (const QString& url : urls) {
        auto* item = new QListWidgetItem(url);
        item->setData(TlItemDelegate::TypeRole, QStringLiteral("custom"));
        item->setData(TlItemDelegate::IsLotlRole, false);
        item->setData(TlItemDelegate::EagerRole, true);
        list->insertItem(list->count() - 1, item);
    }
}

/// The Trust tab's account of what country-signing anchors are installed.
QString cscaSummaryText(SettingsDialog& dlg)
{
    auto* label = dlg.findChild<QLabel*>(QStringLiteral("cscaSummaryLabel"));
    return label != nullptr ? label->text() : QString();
}

/// The Trust tab's account of what the LAST import attempt did.
QString cscaStatusText(SettingsDialog& dlg)
{
    auto* label = dlg.findChild<QLabel*>(QStringLiteral("cscaStatusLabel"));
    return label != nullptr ? label->text() : QString();
}

/// Write @p bytes into @p dir under @p name and answer the path.
QString writeMasterList(const QTemporaryDir& dir, const QString& name, const QByteArray& bytes)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (file.write(bytes) != bytes.size()) {
        return {};
    }
    file.close();
    return path;
}

void clickSigningTabRestoreDefaults(SettingsDialog& dlg)
{
    auto* button = dlg.findChild<QPushButton*>(QStringLiteral("signingRestoreDefaultsButton"));
    ASSERT_NE(button, nullptr);
    button->click();
}

} // namespace

/// The LC widget-test idiom: one QApplication per binary, owned statically by
/// the fixture, because a widget built without one aborts the process under the
/// offscreen platform. The instance guard keeps it to exactly one.
class SettingsConfig1Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QApplication::instance()) {
            // Static: QApplication keeps a reference to argc for its lifetime,
            // so it must outlive this scope.
            static int argc = 0;
            app = new QApplication(argc, nullptr);
        }
        // The dialog still keeps the language and the default output folder in
        // QSettings; keep that half off the developer's real configuration.
        QStandardPaths::setTestModeEnabled(true);

        if (translator != nullptr) {
            return;
        }
        // The anchor summary is a TEMPLATE filled with counts and a date, and
        // with no catalogue loaded qtTrId() answers the bare id -- a string
        // with no %1 in it, which arg() leaves untouched. Every "the summary
        // says 412 anchors" assertion would then compare an id against itself
        // and pass on a build that shows a reader nothing. The catalogue is
        // therefore load-bearing here, not decoration.
        translator = new QTranslator();
        const QString qmDir = QStringLiteral(LIBRECELIK_TRANSLATIONS_DIR_DEFAULT);
        ASSERT_TRUE(translator->load(QStringLiteral("LibreCelik_en"), qmDir))
            << "failed to load LibreCelik_en.qm from " << qmDir.toStdString();
        QCoreApplication::installTranslator(translator);
    }

    static void TearDownTestSuite()
    {
        QCoreApplication::removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    static QApplication* app;
    static QTranslator* translator;
};

QApplication* SettingsConfig1Test::app = nullptr;
QTranslator* SettingsConfig1Test::translator = nullptr;

TEST_F(SettingsConfig1Test, LevelComboWritesWireToken)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.config[QStringLiteral("DefaultLevel")] = QStringLiteral("b-b");
    SettingsDialog dlg(&gw);
    auto* combo = dlg.findChild<QComboBox*>(QStringLiteral("defaultLevelCombo"));
    ASSERT_NE(combo, nullptr);
    combo->setCurrentIndex(combo->findData(QStringLiteral("B_T")));
    invokeSave(dlg); // clicks the OK button by objectName
    ASSERT_FALSE(gw.configWrites.isEmpty());
    EXPECT_EQ(gw.configWrites.last(), qMakePair(QStringLiteral("DefaultLevel"), QVariant(QStringLiteral("b-t"))));
}

TEST_F(SettingsConfig1Test, TslCacheDirControlIsGone)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);
    EXPECT_EQ(dlg.findChild<QLineEdit*>(QStringLiteral("cacheDir")), nullptr);
}

TEST_F(SettingsConfig1Test, OperationTabsDisabledWhileAgentMissing)
{
    FakeAgentGateway gw; // presence defaults to AgentMissing
    SettingsDialog dlg(&gw);
    auto* tabs = dlg.findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);
    EXPECT_TRUE(tabs->isTabEnabled(0));  // General (language) stays usable
    EXPECT_FALSE(tabs->isTabEnabled(1)); // Signing
    EXPECT_FALSE(tabs->isTabEnabled(2)); // Trust
}

TEST_F(SettingsConfig1Test, RestoreDefaultsResetsEveryKeyOfTheTab)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);
    clickSigningTabRestoreDefaults(dlg); // helper: findChild by objectName
    EXPECT_TRUE(gw.configResets.contains(QStringLiteral("DefaultLevel")));
    EXPECT_TRUE(gw.configResets.contains(QStringLiteral("TsaUrls")));
    EXPECT_TRUE(gw.configResets.contains(QStringLiteral("DefaultReason")));
    EXPECT_TRUE(gw.configResets.contains(QStringLiteral("DefaultLocation")));
}

TEST_F(SettingsConfig1Test, NotAuthorizedRefusalRendersOnceAndNeverReprompts)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.nextRefusal = LibreSCRS::AgentClient::SyncError::NotAuthorized;
    SettingsDialog dlg(&gw);
    setTrustTabTslList(dlg, {QStringLiteral("https://x/tl.xml")});
    invokeSave(dlg);
    EXPECT_EQ(gw.configWrites.size(), 1); // exactly one attempt — no retry loop
    EXPECT_TRUE(statusLabelText(dlg).contains(qtTrId("lc-settings-config-unauthorized")));
}

// --- installing country-signing anchors from a master list -------------------
//
// Everything under this application already works: the middleware reads and
// verifies an ICAO master list, the agent installs it behind an authorization
// gate and refuses a rollback. What was missing is the only step a PERSON can
// take, so these cases are about the handover and about what the reader is
// told afterwards -- never about the list's contents, which are none of this
// dialog's business.

// The assertion that separates "hands over a descriptor" from "hands over a
// name". A descriptor is a SECOND REFERENCE TO ONE OPEN FILE DESCRIPTION, not
// a copy of the file, so the receiver's sequential read advances the SENDER's
// file position. A path cannot do that; neither could a dialog that re-opened
// the file by name (or copied its bytes into a fresh descriptor) before handing
// it over -- both would deliver identical bytes while leaving this offset at 0,
// which is why the byte comparison alone proves nothing here.
TEST_F(SettingsConfig1Test, MasterListImportHandsOverADescriptorNotAPath)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);

    QTemporaryDir dir(QStringLiteral("/var/tmp/lc-csca-XXXXXX"));
    ASSERT_TRUE(dir.isValid());
    const QByteArray listBytes = QByteArrayLiteral("LC-ICAO-MASTER-LIST-BYTES");
    const QString path = writeMasterList(dir, QStringLiteral("master-list.ml"), listBytes);
    ASSERT_FALSE(path.isEmpty());

    const int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(::lseek(fd, 0, SEEK_CUR), 0) << "the sender starts at the beginning of the file";

    SettingsDialog dlg(&gw);
    dlg.importMasterList(fd);

    ASSERT_EQ(gw.importedBytes.size(), 1);
    EXPECT_EQ(gw.importedBytes.constFirst(), listBytes) << "the agent must receive the list's bytes verbatim";
    EXPECT_EQ(::lseek(fd, 0, SEEK_CUR), listBytes.size())
        << "the receiver's read must have moved THIS descriptor's offset -- it shares one open file "
           "description with the one handed over. An offset still at 0 means a name (or a freshly "
           "opened descriptor) was passed, which is the failure this assertion exists to catch.";
    ::close(fd);
}

// The other half: the dialog opens what the human chose, and the descriptor it
// opened does not outlive the call. A leaked descriptor pins the file for the
// process's lifetime and is invisible until a long session runs out of them.
TEST_F(SettingsConfig1Test, MasterListImportOpensTheChosenFileAndClosesItsDescriptor)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);

    QTemporaryDir dir(QStringLiteral("/var/tmp/lc-csca-XXXXXX"));
    ASSERT_TRUE(dir.isValid());
    const QByteArray listBytes = QByteArrayLiteral("CHOSEN-BY-THE-READER");
    const QString path = writeMasterList(dir, QStringLiteral("chosen.ml"), listBytes);
    ASSERT_FALSE(path.isEmpty());

    SettingsDialog dlg(&gw);
    dlg.importMasterListFile(path);

    ASSERT_EQ(gw.importedBytes.size(), 1);
    EXPECT_EQ(gw.importedBytes.constFirst(), listBytes);
    ASSERT_EQ(gw.importedFds.size(), 1);
    errno = 0;
    EXPECT_EQ(::fcntl(gw.importedFds.constFirst(), F_GETFD), -1)
        << "the descriptor the dialog opened must not outlive the call";
    EXPECT_EQ(errno, EBADF);
}

// A file this process cannot open never becomes an agent round-trip: there is
// nothing to hand over, and dialling anyway would spend an authorization
// ceremony on a file that was never read.
TEST_F(SettingsConfig1Test, AnUnopenableFileIsSaidWithoutDiallingTheAgent)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);

    QTemporaryDir dir(QStringLiteral("/var/tmp/lc-csca-XXXXXX"));
    ASSERT_TRUE(dir.isValid());

    SettingsDialog dlg(&gw);
    dlg.importMasterListFile(dir.filePath(QStringLiteral("no-such-list.ml")));

    EXPECT_TRUE(gw.importedFds.isEmpty());
    EXPECT_EQ(cscaStatusText(dlg), qtTrId("lc-settings-csca-unreadable"));
}

// An EMPTY state map is the agent saying nothing has been imported -- and it
// is also what a client sees when the agent discarded a stale record because
// its anchor cache had been wiped. The two cannot be told apart from here, and
// both are honestly "nothing installed"; inventing a third state would be a
// claim nobody measured.
TEST_F(SettingsConfig1Test, AnEmptyAnchorStateSaysNothingIsInstalled)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.config[QStringLiteral("CscaAnchorState")] = QVariantMap();

    SettingsDialog dlg(&gw);
    const QString summary = cscaSummaryText(dlg);
    EXPECT_EQ(summary, qtTrId("lc-settings-csca-state-none"));
    EXPECT_FALSE(summary.contains(QRegularExpression(QStringLiteral("[0-9]"))))
        << "a count nobody was given is not a reading: " << qPrintable(summary);
    EXPECT_TRUE(cscaStatusText(dlg).isEmpty()) << "no import has happened, so there is no outcome to report";
}

// The sentence this replaces said what is installed CANNOT be read from here.
// It can now, so nothing may still say otherwise -- an application that keeps
// pleading ignorance after it has been told is worse than one that never
// asked.
TEST_F(SettingsConfig1Test, TheDialogNoLongerSaysTheStateCannotBeRead)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);

    // qtTrId() answers with the bare id when no <message> carries it, which is
    // exactly what a removed catalogue entry looks like -- so the id itself is
    // what must not be on screen either.
    EXPECT_FALSE(cscaSummaryText(dlg).contains(QStringLiteral("cannot be read"), Qt::CaseInsensitive));
    EXPECT_FALSE(cscaSummaryText(dlg).contains(QStringLiteral("lc-settings-csca-state-unknown")));
}

// What the agent already holds, before this dialog has imported anything. The
// property is read-only and served without an import, which is the whole
// reason the sentence above could be replaced.
TEST_F(SettingsConfig1Test, TheDialogSaysWhatTheAgentAlreadyHoldsBeforeAnyImport)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    QVariantMap state;
    state[QStringLiteral("anchors")] = QVariant::fromValue<quint32>(412);
    state[QStringLiteral("issuers")] = QVariant::fromValue<quint32>(78);
    state[QStringLiteral("replayRefusalActive")] = true;
    state[QStringLiteral("signedAt")] =
        QVariant::fromValue<qint64>(QDateTime(QDate(2026, 3, 14), QTime(10, 22), QTimeZone::UTC).toSecsSinceEpoch());
    gw.config[QStringLiteral("CscaAnchorState")] = state;

    SettingsDialog dlg(&gw);
    const QString summary = cscaSummaryText(dlg);
    EXPECT_TRUE(summary.contains(QStringLiteral("412"))) << qPrintable(summary);
    EXPECT_TRUE(summary.contains(QStringLiteral("78"))) << qPrintable(summary);
    EXPECT_TRUE(summary.contains(QStringLiteral("2026-03-14"))) << "the list's own date: " << qPrintable(summary);
    EXPECT_TRUE(summary.contains(qtTrId("lc-settings-csca-rollback-on")));
    // ANCHORS, never roots: the count includes CSCA link certificates.
    EXPECT_FALSE(summary.contains(QStringLiteral("root"), Qt::CaseInsensitive)) << qPrintable(summary);
    EXPECT_TRUE(cscaStatusText(dlg).isEmpty()) << "reading a state is not an import outcome";
}

// An optional key the agent did not send is ABSENT, never zero. `signedAt`
// absent means the accepted list carried no CMS signing time -- so there is no
// date to print, and no later list can be checked for rolling the anchors
// back. Printing an epoch-valued stand-in would read as a list signed in 1970;
// printing an empty line would read as a date nobody could be bothered with.
TEST_F(SettingsConfig1Test, AnAbsentSignedAtPrintsNoDateLineAndSaysWhyRollbackCannotBeChecked)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    QVariantMap state;
    state[QStringLiteral("anchors")] = QVariant::fromValue<quint32>(9);
    state[QStringLiteral("issuers")] = QVariant::fromValue<quint32>(3);
    state[QStringLiteral("replayRefusalActive")] = false; // and no signedAt key at all
    gw.config[QStringLiteral("CscaAnchorState")] = state;

    SettingsDialog dlg(&gw);
    const QString summary = cscaSummaryText(dlg);
    EXPECT_TRUE(summary.contains(qtTrId("lc-settings-csca-rollback-off"))) << qPrintable(summary);
    EXPECT_FALSE(summary.contains(qtTrId("lc-settings-csca-rollback-on")));
    EXPECT_FALSE(summary.contains(QStringLiteral("1970")))
        << "an absent signing time printed as the epoch: " << qPrintable(summary);
    // Two lines: the counts, and why rollback cannot be checked. A date line
    // would be a third, and an empty one would still be a line.
    EXPECT_EQ(summary.count(QLatin1Char('\n')), 1) << "an undated list grew a date line: " << qPrintable(summary);
}

// What an accepted list is worth saying: how many ANCHORS -- never "roots",
// because the count includes CSCA link certificates -- how many distinct
// issuers, the list's own date when it carried one, and that rollback refusal
// is operating.
TEST_F(SettingsConfig1Test, AnchorSummaryCountsAnchorsAndIssuersAndNeverSaysRoots)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.scriptedAnchorState.anchors = 412;
    gw.scriptedAnchorState.issuers = 78;
    gw.scriptedAnchorState.replayRefusalActive = true;
    gw.scriptedAnchorState.signedAt = QDateTime(QDate(2026, 3, 14), QTime(10, 22), QTimeZone::UTC);

    const int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    SettingsDialog dlg(&gw);
    dlg.importMasterList(fd);
    ::close(fd);

    const QString summary = cscaSummaryText(dlg);
    EXPECT_TRUE(summary.contains(QStringLiteral("412"))) << qPrintable(summary);
    EXPECT_TRUE(summary.contains(QStringLiteral("78"))) << qPrintable(summary);
    EXPECT_TRUE(summary.contains(QStringLiteral("2026"))) << "the list's own date: " << qPrintable(summary);
    EXPECT_TRUE(summary.contains(qtTrId("lc-settings-csca-rollback-on")));
    EXPECT_FALSE(summary.contains(QStringLiteral("root"), Qt::CaseInsensitive))
        << "the count includes CSCA link certificates, which are not roots: " << qPrintable(summary);
    EXPECT_EQ(cscaStatusText(dlg), qtTrId("lc-settings-csca-installed"));
}

// The FALSE of replayRefusalActive is the value worth surfacing: an accepted
// list with no signing time means a later list cannot be checked for rolling
// the anchors back at all, and silence leaves a reader unable to tell that
// from "this is safe".
TEST_F(SettingsConfig1Test, AnUndatedListSaysRollbackCannotBeChecked)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.scriptedAnchorState.anchors = 9;
    gw.scriptedAnchorState.issuers = 3;
    gw.scriptedAnchorState.replayRefusalActive = false; // and signedAt stays invalid

    const int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    SettingsDialog dlg(&gw);
    dlg.importMasterList(fd);
    ::close(fd);

    const QString summary = cscaSummaryText(dlg);
    EXPECT_TRUE(summary.contains(qtTrId("lc-settings-csca-rollback-off"))) << qPrintable(summary);
    EXPECT_FALSE(summary.contains(qtTrId("lc-settings-csca-rollback-on")));
    // An undated list contributes no date line -- never an epoch-valued
    // stand-in, which would read as a real signing time.
    EXPECT_EQ(summary.count(QLatin1Char('\n')), 1)
        << "an undated list must not grow a date line: " << qPrintable(summary);
}

// "Strictly newer" admits no equality, so handing over the same file again is
// refused -- with a NAMED error, which is the whole reason a person can be told
// something they can act on instead of a wire spelling.
TEST_F(SettingsConfig1Test, ReimportingTheSameListSaysItIsAlreadyInstalled)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.nextImportRefusal = LibreSCRS::AgentClient::SyncError::MasterListReplayed;

    const int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    SettingsDialog dlg(&gw);
    dlg.importMasterList(fd);
    ::close(fd);

    const QString status = cscaStatusText(dlg);
    EXPECT_EQ(status, qtTrId("lc-settings-csca-replayed"));
    EXPECT_FALSE(status.contains(QStringLiteral("MasterListReplayed")))
        << "a wire name is not something a reader can act on";
    // Nothing was installed and nothing already held was given up, so the
    // account of what the agent holds is exactly what it was before the
    // attempt -- here, nothing.
    EXPECT_EQ(cscaSummaryText(dlg), qtTrId("lc-settings-csca-state-none"));
}

TEST_F(SettingsConfig1Test, ARefusedAuthorizationForAnImportIsSaidInWords)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.nextImportRefusal = LibreSCRS::AgentClient::SyncError::NotAuthorized;

    const int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    SettingsDialog dlg(&gw);
    dlg.importMasterList(fd);
    ::close(fd);

    EXPECT_EQ(cscaStatusText(dlg), qtTrId("lc-settings-config-unauthorized"));
}

// Every "this file is not a usable master list" refusal the agent names is
// outside the closed error vocabulary and arrives generically. One sentence
// covers them, and it offers the only move that helps: a different file.
TEST_F(SettingsConfig1Test, AFileTheAgentWillNotInstallOffersADifferentFile)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.nextImportRefusal = LibreSCRS::AgentClient::SyncError::CommunicationError;

    const int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    SettingsDialog dlg(&gw);
    dlg.importMasterList(fd);
    ::close(fd);

    EXPECT_EQ(cscaStatusText(dlg), qtTrId("lc-settings-csca-refused"));
    EXPECT_EQ(cscaSummaryText(dlg), qtTrId("lc-settings-csca-state-none"));
}

// The Trust tab goes dark with no agent, and the import must observe the same
// rule the rest of the tab does rather than opening a file for nobody.
TEST_F(SettingsConfig1Test, ImportDoesNothingWhileTheAgentIsAway)
{
    FakeAgentGateway gw; // presence defaults to AgentMissing

    QTemporaryDir dir(QStringLiteral("/var/tmp/lc-csca-XXXXXX"));
    ASSERT_TRUE(dir.isValid());
    const QString path = writeMasterList(dir, QStringLiteral("unused.ml"), QByteArrayLiteral("BYTES"));
    ASSERT_FALSE(path.isEmpty());

    SettingsDialog dlg(&gw);
    dlg.importMasterListFile(path);

    EXPECT_TRUE(gw.importedFds.isEmpty());
    EXPECT_EQ(cscaSummaryText(dlg), qtTrId("lc-settings-csca-state-none"));
}

// The import affordance is on the Trust tab, where the anchors it installs are
// accounted for -- and it is what the five eMRTD signer-reason sentences now
// name. A reason that sends a reader to a control that is not there is worse
// than one that names only the action.
TEST_F(SettingsConfig1Test, TheTrustTabCarriesTheImportAffordanceTheReasonsNameNow)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);

    auto* button = dlg.findChild<QPushButton*>(QStringLiteral("cscaImportButton"));
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text(), qtTrId("lc-settings-csca-import"));
    // No sources list: nothing fetches from one, and a control that silently
    // does nothing is worse than an absent one.
    EXPECT_EQ(dlg.findChild<QListWidget*>(QStringLiteral("cscaList")), nullptr);
}
