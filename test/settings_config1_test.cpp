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
#include "settings/masterlistprobe.h"
#include "settings/tlitemdelegate.h"

#include <LibreSCRS/AgentClient/SyncError.h>

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QIODevice>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTime>
#include <QTimeZone>
#include <QTranslator>
#include <QVBoxLayout>
#include <QVariant>
#include <QVariantList>

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

// --- the sentences have to be all the way there on the first show ------------
//
// A word-wrapped QLabel answers a layout with ONE LINE when asked for its
// minimum height. So the moment a tab is shorter than the sum of what its
// children would like, the layout squeezes the sentences to one line each and
// cuts the rest off mid-glyph -- honouring every constraint it was given, and
// telling nobody. It then "corrects itself" the next time any content changes,
// which is how the defect was reported: cut on arrival, right after an error
// message appeared.

/// Let the layout reach its fixed point, the way a running event loop does
/// before the first paint.
void settleLayout()
{
    for (int round = 0; round < 8; ++round) {
        QCoreApplication::processEvents();
    }
}

TEST_F(SettingsConfig1Test, NoWrappedSentenceOpensCutOff)
{
    // The pressure is the precondition, not decoration: at the offscreen
    // default font the Trust tab fits inside the dialog's opening size and
    // nothing is squeezed, so the assertion below would pass over the very bug
    // it exists to catch. Four points up is what puts the tab under pressure.
    const QFont original = QApplication::font();
    QFont larger = original;
    larger.setPointSizeF(original.pointSizeF() + 4.0);
    QApplication::setFont(larger);
    const auto restoreFont = qScopeGuard([&original]() { QApplication::setFont(original); });

    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);
    auto* tabs = dlg.findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);

    // The path a reader takes: the dialog opens on General, they click across.
    dlg.show();
    settleLayout();
    tabs->setCurrentIndex(2); // Trust
    settleLayout();

    int measured = 0;
    for (QLabel* label : dlg.findChildren<QLabel*>()) {
        if (!label->isVisible() || !label->wordWrap()) {
            continue;
        }
        ++measured;
        // The PROPERTY, not a pixel count: whatever the font and the catalogue,
        // a label must be at least as tall as its own wrapping needs at the
        // width it was actually given.
        EXPECT_GE(label->height(), label->heightForWidth(label->width()))
            << "cut off on the first show: " << qPrintable(label->objectName()) << " was given " << label->height()
            << "px for text that wraps to " << label->heightForWidth(label->width()) << "px at " << label->width()
            << "px wide";
    }
    EXPECT_GE(measured, 4) << "the Trust tab carries four wrapped sentences; measuring none would make this "
                              "test pass on a dialog that renders nothing";
}

// The second half of the report: it "lays out correctly" only once a message
// appears. Whatever is true after that message must already be true before it,
// or the first show is a different (worse) dialog than the one a reader ends up
// looking at.
TEST_F(SettingsConfig1Test, AMessageAppearingChangesNoSentenceThatWasAlreadyRight)
{
    const QFont original = QApplication::font();
    QFont larger = original;
    larger.setPointSizeF(original.pointSizeF() + 4.0);
    QApplication::setFont(larger);
    const auto restoreFont = qScopeGuard([&original]() { QApplication::setFont(original); });

    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.nextImportRefusal = LibreSCRS::AgentClient::SyncError::CommunicationError;

    SettingsDialog dlg(&gw);
    auto* tabs = dlg.findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);
    dlg.show();
    settleLayout();
    tabs->setCurrentIndex(2);
    settleLayout();

    const int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    dlg.importMasterList(fd);
    ::close(fd);
    settleLayout();

    for (QLabel* label : dlg.findChildren<QLabel*>()) {
        if (!label->isVisible() || !label->wordWrap()) {
            continue;
        }
        EXPECT_GE(label->height(), label->heightForWidth(label->width()))
            << "still cut off after the message: " << qPrintable(label->objectName());
    }
}

// --- the shape a reader actually downloads -----------------------------------
//
// The dialog tells a reader to fetch the latest collection of eMRTD CSCA master
// lists from the ICAO Public Key Directory. What that portal serves is an LDAP
// interchange file (RFC 2849), and what is inside it is not one master list but
// many, each signed by its own publisher. The importer installs ONE signed list
// with ONE signer to pin, so a collection is not something it can accept -- and
// the reader who followed the instruction exactly deserves to be told that in
// those terms, not "choose a different file".

namespace {

/// One PKD-shaped record: a base64 binary attribute carrying @p der, folded
/// across continuation lines the way a real export folds them.
QByteArray ldifRecord(const QByteArray& attribute, const QByteArray& der)
{
    const QByteArray encoded = der.toBase64();
    QByteArray folded;
    constexpr int kLineWidth = 76;
    for (int at = 0; at < encoded.size(); at += kLineWidth) {
        folded += (at == 0 ? QByteArray() : QByteArrayLiteral("\n ")) + encoded.mid(at, kLineWidth);
    }
    return attribute + QByteArrayLiteral(":: ") + folded + QByteArrayLiteral("\n");
}

/// A minimal CMS ContentInfo carrying id-signedData: outer SEQUENCE, then the
/// OID. Enough to be recognised as one signed object, which is all the probe
/// claims to see -- it never verifies anything, and must not.
QByteArray signedDataObject(int padding)
{
    QByteArray body = QByteArrayLiteral("\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x07\x02");
    body += QByteArray(padding, 'A');
    QByteArray der;
    der += char(0x30);
    der += char(body.size()); // short form: the bodies here stay under 128 bytes
    der += body;
    return der;
}

QByteArray pkdCollection(int lists)
{
    QByteArray ldif = QByteArrayLiteral("version: 1\n\n");
    ldif += QByteArrayLiteral("dn: dc=data,dc=download,dc=pkd,dc=icao,dc=int\ndc: data\nobjectclass: top\n\n");
    for (int i = 0; i < lists; ++i) {
        ldif += QByteArrayLiteral("dn: o=Master Lists,c=RS,dc=data,dc=download,dc=pkd,dc=icao,dc=int\n");
        ldif += QByteArrayLiteral("objectclass: inetOrgPerson\n");
        ldif += ldifRecord(QByteArrayLiteral("pkdMasterListContent;binary"), signedDataObject(i + 1));
        ldif += QByteArrayLiteral("\n");
    }
    return ldif;
}

} // namespace

// The whole point: the reader downloaded the right file, and the sentence they
// get back has to say so -- with the count, which is the evidence that it is a
// COLLECTION rather than a list. And it must never reach the agent: the agent's
// answer would be a correct but useless "not a master list", bought with an
// authorization ceremony and one of its per-caller import allowances.
TEST_F(SettingsConfig1Test, ThePkdCollectionIsNamedAsACollectionAndNeverDialledOut)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);

    QTemporaryDir dir(QStringLiteral("/var/tmp/lc-csca-XXXXXX"));
    ASSERT_TRUE(dir.isValid());
    const QString path = writeMasterList(dir, QStringLiteral("icaopkd-002-complete.ldif"), pkdCollection(28));
    ASSERT_FALSE(path.isEmpty());

    SettingsDialog dlg(&gw);
    dlg.importMasterListFile(path);

    EXPECT_TRUE(gw.importedFds.isEmpty()) << "a collection the agent could only refuse must not cost an "
                                             "authorization ceremony to be refused";
    const QString status = cscaStatusText(dlg);
    EXPECT_EQ(status, qtTrId("lc-settings-csca-ldif-collection").arg(28));
    EXPECT_TRUE(status.contains(QStringLiteral("28"))) << qPrintable(status);
    EXPECT_NE(status, qtTrId("lc-settings-csca-refused")) << "the collection is a different situation from a file "
                                                             "that is not a master list, and reads differently";
    EXPECT_EQ(cscaSummaryText(dlg), qtTrId("lc-settings-csca-state-none")) << "a refusal installs nothing";
}

// A `.ldif` extension is a hint and not a contract, in both directions: the
// shape decides. A collection named `.ml` is still a collection.
TEST_F(SettingsConfig1Test, TheShapeDecidesNotTheExtension)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);

    QTemporaryDir dir(QStringLiteral("/var/tmp/lc-csca-XXXXXX"));
    ASSERT_TRUE(dir.isValid());
    const QString path = writeMasterList(dir, QStringLiteral("looks-like-a-list.ml"), pkdCollection(3));
    ASSERT_FALSE(path.isEmpty());

    SettingsDialog dlg(&gw);
    dlg.importMasterListFile(path);

    EXPECT_TRUE(gw.importedFds.isEmpty());
    EXPECT_EQ(cscaStatusText(dlg), qtTrId("lc-settings-csca-ldif-collection").arg(3));
}

// The other half of the same rule: a file that does NOT parse as LDIF goes to
// the agent exactly as before, whatever it is called. The trust boundary is the
// agent's verb, and this dialog must not start deciding what a master list is.
TEST_F(SettingsConfig1Test, AFileThatIsNotLdifStillReachesTheAgentWhateverItIsNamed)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);

    QTemporaryDir dir(QStringLiteral("/var/tmp/lc-csca-XXXXXX"));
    ASSERT_TRUE(dir.isValid());
    const QByteArray listBytes = QByteArrayLiteral("\x30\x82\x01\x00NOT-LDIF-AT-ALL\x00\x01\x02");
    const QString path = writeMasterList(dir, QStringLiteral("master.ldif"), listBytes);
    ASSERT_FALSE(path.isEmpty());

    SettingsDialog dlg(&gw);
    dlg.importMasterListFile(path);

    ASSERT_EQ(gw.importedBytes.size(), 1) << "the agent decides what a master list is, not this dialog";
    EXPECT_EQ(gw.importedBytes.constFirst(), listBytes)
        << "the probe must not consume the descriptor the agent then reads from";
}

// An LDIF with nothing signed in it is a third situation, and it reads as one:
// neither "not a master list" nor "a collection we cannot install yet".
TEST_F(SettingsConfig1Test, AnLdifCarryingNoSignedObjectIsSaidSeparately)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);

    QTemporaryDir dir(QStringLiteral("/var/tmp/lc-csca-XXXXXX"));
    ASSERT_TRUE(dir.isValid());
    const QByteArray ldif = QByteArrayLiteral("version: 1\n\ndn: c=RS,dc=data\nc: RS\nobjectclass: country\n");
    const QString path = writeMasterList(dir, QStringLiteral("countries.ldif"), ldif);
    ASSERT_FALSE(path.isEmpty());

    SettingsDialog dlg(&gw);
    dlg.importMasterListFile(path);

    EXPECT_TRUE(gw.importedFds.isEmpty());
    const QString status = cscaStatusText(dlg);
    EXPECT_EQ(status, qtTrId("lc-settings-csca-ldif-empty"));
    EXPECT_NE(status, qtTrId("lc-settings-csca-ldif-collection").arg(0));
    EXPECT_NE(status, qtTrId("lc-settings-csca-refused"));
}

// The three refusals a reader can meet are three different sentences. Asserting
// they DIFFER is the check that survives a rewording of any one of them.
TEST_F(SettingsConfig1Test, TheThreeRefusalsDoNotShareASentence)
{
    const QString refused = qtTrId("lc-settings-csca-refused");
    const QString collection = qtTrId("lc-settings-csca-ldif-collection").arg(28);
    const QString empty = qtTrId("lc-settings-csca-ldif-empty");

    EXPECT_NE(refused, collection);
    EXPECT_NE(refused, empty);
    EXPECT_NE(collection, empty);
    for (const QString& sentence : {refused, collection, empty}) {
        EXPECT_FALSE(sentence.startsWith(QStringLiteral("lc-settings-")))
            << "the catalogue did not load, so these are ids and not sentences";
        // "Choose a different file" was the whole of the old answer, and it is
        // what sent a reader who had downloaded exactly the right thing looking
        // for a different one.
        EXPECT_FALSE(sentence.contains(QStringLiteral("Choose a different file"))) << qPrintable(sentence);
    }
}

// --- the probe itself, at the edges RFC 2849 actually has ---------------------

TEST_F(SettingsConfig1Test, ProbeUnfoldsContinuationLinesBeforeDecoding)
{
    using namespace librecelik::settings;
    // The base64 of a real list spans hundreds of folded lines; a probe that
    // decoded them one at a time would see hundreds of unrecognisable
    // fragments and count nothing.
    const auto probe = probeMasterListFile(pkdCollection(2));
    EXPECT_EQ(probe.kind, MasterListFileKind::LdifCollection);
    EXPECT_EQ(probe.signedObjects, 2);
}

TEST_F(SettingsConfig1Test, ProbeReadsCrlfAndCommentsAndAMissingFinalNewline)
{
    using namespace librecelik::settings;
    QByteArray ldif = QByteArrayLiteral("# exported by the portal\r\nversion: 1\r\n\r\ndn: c=RS,dc=data\r\nc: RS");
    const auto probe = probeMasterListFile(ldif);
    EXPECT_EQ(probe.kind, MasterListFileKind::LdifWithoutLists);
    EXPECT_EQ(probe.signedObjects, 0);
}

TEST_F(SettingsConfig1Test, ProbeCountsTheBerIndefiniteLengthEncodingToo)
{
    using namespace librecelik::settings;
    // One of the lists in the real collection is BER, not DER: `30 80` with the
    // end-of-contents octets closing it. Counting only the definite form would
    // undercount the collection by one and put a wrong number on screen.
    QByteArray ber;
    ber += char(0x30);
    ber += char(0x80);
    ber += QByteArrayLiteral("\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x07\x02");
    ber += QByteArrayLiteral("\xa0\x80payload\x00\x00");
    ber += QByteArrayLiteral("\x00\x00");

    QByteArray ldif = QByteArrayLiteral("dn: o=Master Lists,c=RS\n");
    ldif += ldifRecord(QByteArrayLiteral("pkdMasterListContent;binary"), ber);
    const auto probe = probeMasterListFile(ldif);
    EXPECT_EQ(probe.kind, MasterListFileKind::LdifCollection);
    EXPECT_EQ(probe.signedObjects, 1);
}

TEST_F(SettingsConfig1Test, ProbeCountsSignedObjectsStructurallyNotByAttributeName)
{
    using namespace librecelik::settings;
    // The real collection carries a base64 `cn` beside its 28 lists. Counting
    // every base64 value would answer 29 and print a number nobody can check;
    // counting by attribute name would break the day the publisher renames it.
    QByteArray ldif = QByteArrayLiteral("dn: o=Master Lists,c=RS\n");
    ldif += ldifRecord(QByteArrayLiteral("cn"), QByteArrayLiteral("  a name needing base64  "));
    ldif += ldifRecord(QByteArrayLiteral("someFutureAttributeName;binary"), signedDataObject(4));
    const auto probe = probeMasterListFile(ldif);
    EXPECT_EQ(probe.kind, MasterListFileKind::LdifCollection);
    EXPECT_EQ(probe.signedObjects, 1) << "the base64 cn is not a signed object and must not be counted";
}

TEST_F(SettingsConfig1Test, ProbeRefusesToCallBinaryOrDnLessTextLdif)
{
    using namespace librecelik::settings;
    // A master list is binary and full of zero octets.
    EXPECT_EQ(probeMasterListFile(QByteArrayLiteral("\x30\x82\x04\x00\x06\x09\x2a\x86")).kind,
              MasterListFileKind::NotLdif);
    // Text with colons in it is not a directory export.
    EXPECT_EQ(probeMasterListFile(QByteArrayLiteral("Subject: hello\nFrom: nobody\n")).kind,
              MasterListFileKind::NotLdif)
        << "no dn, so this is some other colon-separated text";
    // PEM is text, has no colon-separated attributes at all.
    EXPECT_EQ(probeMasterListFile(QByteArrayLiteral("-----BEGIN CMS-----\nMIIB\n-----END CMS-----\n")).kind,
              MasterListFileKind::NotLdif);
    EXPECT_EQ(probeMasterListFile(QByteArray()).kind, MasterListFileKind::NotLdif);
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

// Two settings live on this tab and each has to read as one: the trusted lists
// a signature is validated against, and the country-signing anchors a travel
// document is checked against. The anchor half used to be a heading and three
// loose lines under the list widget, which read as a footnote to the list
// rather than as the other half of the tab. The framing is the assertion:
// each is a titled box, and the widgets that belong to it live inside it.
TEST_F(SettingsConfig1Test, TheTrustTabReadsAsTwoTitledSections)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);

    auto* tlGroup = dlg.findChild<QGroupBox*>(QStringLiteral("tlGroup"));
    auto* cscaGroup = dlg.findChild<QGroupBox*>(QStringLiteral("cscaGroup"));
    ASSERT_NE(tlGroup, nullptr);
    ASSERT_NE(cscaGroup, nullptr);
    EXPECT_EQ(tlGroup->title(), qtTrId("lc-settings-tl-servers"));
    EXPECT_EQ(cscaGroup->title(), qtTrId("lc-settings-csca-anchors"));
    // A section title is a heading, not the left half of a "label: value"
    // pair -- and there is no value coming after it.
    EXPECT_FALSE(tlGroup->title().endsWith(QLatin1Char(':'))) << qPrintable(tlGroup->title());
    EXPECT_FALSE(cscaGroup->title().endsWith(QLatin1Char(':'))) << qPrintable(cscaGroup->title());

    // Each section OWNS its widgets. That is what makes a frame mean anything:
    // findChild() on the dialog would find them wherever they happened to sit,
    // so the search has to start at the box that claims them.
    EXPECT_NE(tlGroup->findChild<QListWidget*>(QStringLiteral("tlList")), nullptr);
    EXPECT_NE(cscaGroup->findChild<QLabel*>(QStringLiteral("cscaSummaryLabel")), nullptr);
    EXPECT_NE(cscaGroup->findChild<QLabel*>(QStringLiteral("cscaStatusLabel")), nullptr);
    EXPECT_NE(cscaGroup->findChild<QLabel*>(QStringLiteral("cscaHelpLabel")), nullptr);
    // Neither section may swallow the other's material.
    EXPECT_EQ(tlGroup->findChild<QLabel*>(QStringLiteral("cscaSummaryLabel")), nullptr);
    EXPECT_EQ(cscaGroup->findChild<QListWidget*>(QStringLiteral("tlList")), nullptr);
}

// The import installs the anchors, so it belongs inside the anchors' own
// frame, under the sentence that says what to import. The restore hands the
// whole tab's keys back to the agent, so it stays outside both sections. They
// used to be two right-aligned buttons stacked in a column with nothing on
// screen to say which of them acted on what.
TEST_F(SettingsConfig1Test, EachTrustTabButtonSitsWithTheThingItActsOn)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);

    auto* cscaGroup = dlg.findChild<QGroupBox*>(QStringLiteral("cscaGroup"));
    auto* tlGroup = dlg.findChild<QGroupBox*>(QStringLiteral("tlGroup"));
    ASSERT_NE(cscaGroup, nullptr);
    ASSERT_NE(tlGroup, nullptr);

    auto* importButton = dlg.findChild<QPushButton*>(QStringLiteral("cscaImportButton"));
    auto* restoreButton = dlg.findChild<QPushButton*>(QStringLiteral("trustRestoreDefaultsButton"));
    ASSERT_NE(importButton, nullptr);
    ASSERT_NE(restoreButton, nullptr);

    EXPECT_EQ(cscaGroup->findChild<QPushButton*>(QStringLiteral("cscaImportButton")), importButton);
    EXPECT_EQ(cscaGroup->findChild<QPushButton*>(QStringLiteral("trustRestoreDefaultsButton")), nullptr);
    EXPECT_EQ(tlGroup->findChild<QPushButton*>(QStringLiteral("trustRestoreDefaultsButton")), nullptr);
    EXPECT_EQ(tlGroup->findChild<QPushButton*>(QStringLiteral("cscaImportButton")), nullptr);
}

// The two agent-backed tabs each end with a restore that hands back every key
// the TAB owns -- not the box it happens to sit under. Standing loose under the
// last setting it read as a third control of that setting on one tab and as
// belonging to nothing at all on the other. The same rule now closes both tabs
// above the same right-aligned row, so the two answer "what does this reach"
// the same way.
TEST_F(SettingsConfig1Test, BothTabsCloseTheirRestoreRowTheSameWay)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);

    auto* tabs = dlg.findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);

    for (const auto& tab : {qMakePair(1, QStringLiteral("signingRestoreDefaultsButton")),
                            qMakePair(2, QStringLiteral("trustRestoreDefaultsButton"))}) {
        QWidget* page = tabs->widget(tab.first);
        ASSERT_NE(page, nullptr);
        auto* button = page->findChild<QPushButton*>(tab.second);
        ASSERT_NE(button, nullptr) << qPrintable(tab.second);
        // On the PAGE, not inside one of its framed settings: what it resets is
        // the tab.
        EXPECT_EQ(button->parentWidget(), page) << qPrintable(tab.second) << " moved inside a setting's own frame";

        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        ASSERT_NE(layout, nullptr);
        int ruleAt = -1;
        const QList<QFrame*> frames = page->findChildren<QFrame*>(QString(), Qt::FindDirectChildrenOnly);
        for (QFrame* frame : frames) {
            if (frame->frameShape() == QFrame::HLine)
                ruleAt = layout->indexOf(frame);
        }
        ASSERT_GE(ruleAt, 0) << qPrintable(tab.second) << " stands with nothing between it and the settings above";

        int rowAt = -1;
        for (int i = 0; i < layout->count(); ++i) {
            QLayout* row = layout->itemAt(i)->layout();
            if (row != nullptr && row->indexOf(button) >= 0)
                rowAt = i;
        }
        ASSERT_GE(rowAt, 0) << qPrintable(tab.second) << " is not in a row of its own";
        EXPECT_EQ(rowAt, ruleAt + 1) << qPrintable(tab.second) << " is not the row directly under the rule";
    }
}

// Nobody can use the import without a master list, and nothing on this screen
// used to say what one is or where to get it -- which made the whole feature
// unreachable for a reader who had never heard the term. Exactly ONE address
// is named: the publisher's own public download portal, the one this project
// has actually checked. Other issuers publish lists too, but an address that
// turns out to be wrong spends a reader's time and their trust in everything
// else the dialog says.
TEST_F(SettingsConfig1Test, TheTrustTabSaysWhatAMasterListIsAndWhereItComesFrom)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);

    // Three kinds of thing, three labels: what it IS, WHERE it comes from, and
    // WHY nothing here downloads it.
    auto* what = dlg.findChild<QLabel*>(QStringLiteral("cscaWhatLabel"));
    auto* help = dlg.findChild<QLabel*>(QStringLiteral("cscaHelpLabel"));
    auto* manual = dlg.findChild<QLabel*>(QStringLiteral("cscaManualLabel"));
    ASSERT_NE(what, nullptr);
    ASSERT_NE(help, nullptr);
    ASSERT_NE(manual, nullptr);
    const QString text = help->text();

    // What the thing IS -- the name on its own means nothing the first time.
    EXPECT_EQ(what->text(), qtTrId("lc-settings-csca-what"));
    // WHERE it comes from. The sentence carries the address as a placeholder,
    // so what must be on screen is both halves of it around a filled-in %1.
    const QStringList whereParts = qtTrId("lc-settings-csca-where").split(QStringLiteral("%1"));
    ASSERT_EQ(whereParts.size(), 2) << "the where-sentence lost its address placeholder";
    for (const QString& part : whereParts) {
        EXPECT_TRUE(text.contains(part)) << qPrintable(part);
    }
    // WHY nothing downloads it here: the portal asks a person to accept terms.
    // Said out loud so the absent automatic fetch reads as a decision.
    EXPECT_EQ(manual->text(), qtTrId("lc-settings-csca-manual"));

    // The address, both as something to click and as something to read off the
    // screen and type into a browser.
    EXPECT_TRUE(text.contains(QStringLiteral("href=\"https://pkddownload.icao.int/\""))) << qPrintable(text);
    EXPECT_TRUE(text.contains(QStringLiteral(">https://pkddownload.icao.int/<"))) << qPrintable(text);
    EXPECT_TRUE(help->openExternalLinks());
    EXPECT_FALSE(text.contains(QStringLiteral("%1")))
        << "an unfilled placeholder reached the screen: " << qPrintable(text);

    // Still exactly ONE address, counted across all three now that they are
    // three widgets: splitting the block must not have been a chance to name a
    // second place a master list might come from.
    const QString whole = what->text() + help->text() + manual->text();
    EXPECT_EQ(whole.count(QStringLiteral("http")), 2)
        << "one address, named twice in one anchor tag: " << qPrintable(whole);
}

// The three sentences carry three different kinds of information, and the one
// a reader opened this frame for is the INSTRUCTION. It keeps the body voice;
// the definition and the reason are set in the application's quiet treatment --
// one point down, placeholder colour -- so an eye scanning for the address is
// not made to read a wall of identical text to find it.
TEST_F(SettingsConfig1Test, TheDownloadInstructionIsLouderThanTheContextAroundIt)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);

    auto* what = dlg.findChild<QLabel*>(QStringLiteral("cscaWhatLabel"));
    auto* help = dlg.findChild<QLabel*>(QStringLiteral("cscaHelpLabel"));
    auto* manual = dlg.findChild<QLabel*>(QStringLiteral("cscaManualLabel"));
    ASSERT_NE(what, nullptr);
    ASSERT_NE(help, nullptr);
    ASSERT_NE(manual, nullptr);

    for (QLabel* quiet : {what, manual}) {
        EXPECT_LT(quiet->font().pointSizeF(), help->font().pointSizeF())
            << qPrintable(quiet->objectName()) << " is set at the instruction's own size";
        // A palette ROLE, not a colour literal: it has to follow a theme change.
        EXPECT_EQ(quiet->foregroundRole(), QPalette::PlaceholderText) << qPrintable(quiet->objectName());
    }
    EXPECT_NE(help->foregroundRole(), QPalette::PlaceholderText) << "the instruction went quiet with the context";

    // And every one of them still wraps: a hierarchy that clips is not one.
    EXPECT_TRUE(what->wordWrap());
    EXPECT_TRUE(help->wordWrap());
    EXPECT_TRUE(manual->wordWrap());
}

// The count is a READING of what the agent holds; the paragraph under it is
// ADVICE about changing that. They ran together as one block of identical text,
// so the count read as the first sentence of the instructions. A rule between
// them is what says they are two different things.
TEST_F(SettingsConfig1Test, ARuleSeparatesWhatIsHeldFromWhatToDoAboutIt)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);

    auto* group = dlg.findChild<QGroupBox*>(QStringLiteral("cscaGroup"));
    ASSERT_NE(group, nullptr);
    auto* layout = qobject_cast<QVBoxLayout*>(group->layout());
    ASSERT_NE(layout, nullptr);

    const auto indexOf = [layout](QWidget* widget) { return layout->indexOf(widget); };
    const int summaryAt = indexOf(dlg.findChild<QLabel*>(QStringLiteral("cscaSummaryLabel")));
    const int whatAt = indexOf(dlg.findChild<QLabel*>(QStringLiteral("cscaWhatLabel")));
    ASSERT_GE(summaryAt, 0);
    ASSERT_GE(whatAt, 0);

    int ruleAt = -1;
    const QList<QFrame*> frames = group->findChildren<QFrame*>(QString(), Qt::FindDirectChildrenOnly);
    for (QFrame* frame : frames) {
        if (frame->frameShape() == QFrame::HLine)
            ruleAt = indexOf(frame);
    }
    ASSERT_GE(ruleAt, 0) << "nothing separates the anchor account from the advice under it";
    EXPECT_GT(ruleAt, summaryAt) << "the rule landed above the reading it is meant to close";
    EXPECT_LT(ruleAt, whatAt) << "the rule landed below the advice it is meant to open";
}

// Both list boxes size to the rows they hold. A box that expands into whatever
// vertical space is going spare shows one entry stranded at the top of a frame
// five times its height, which reads as a list that failed to load rather than
// as a list with one thing in it.
TEST_F(SettingsConfig1Test, ListBoxesSizeToTheirRowsRatherThanToTheTab)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    QVariantList sources;
    sources.append(QVariant(QVariantList{QStringLiteral("https://example.test/lotl.xml"), false, true}));
    gw.config[QStringLiteral("TslSources")] = sources;
    gw.config[QStringLiteral("TsaUrls")] = QStringList{QStringLiteral("https://tsa.example.test/")};

    SettingsDialog dlg(&gw);
    auto* tabs = dlg.findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);
    dlg.show();

    for (const QString& name : {QStringLiteral("tlList"), QStringLiteral("tsaList")}) {
        // The box has to be on the VISIBLE page for the tab widget to have laid
        // it out at all; a page that was never current keeps its widgets at
        // whatever size they were constructed with, and a height nobody
        // computed cannot be evidence about how tall this box grows.
        tabs->setCurrentIndex(name == QStringLiteral("tsaList") ? 1 : 2);
        dlg.resize(680, 900); // far more height than either list has rows for
        QCoreApplication::processEvents();

        auto* list = dlg.findChild<QListWidget*>(name);
        ASSERT_NE(list, nullptr) << qPrintable(name);
        ASSERT_EQ(list->count(), 2) << "one entry and the add sentinel"; // NOLINT

        int rows = 2 * list->frameWidth();
        int tallest = 0;
        for (int row = 0; row < list->count(); ++row) {
            rows += list->sizeHintForRow(row);
            tallest = qMax(tallest, list->sizeHintForRow(row));
        }
        // Never SHORTER than its rows: a box that clips its own entries is the
        // failure this sizing exists to avoid.
        EXPECT_GE(list->height(), rows) << qPrintable(name) << " clips the rows it holds";
        // And never taller than the ceiling, however much room the tab has.
        EXPECT_LE(list->height(), 2 * list->frameWidth() + 6 * tallest)
            << qPrintable(name) << " grew past its ceiling into the spare height";
    }
}

// The minimum size is a size the dialog must actually be able to DRAW at. The
// one shipped before this test could not: at it, a third of the download
// paragraph was cut off in every anchor state and every catalogue. A word-
// wrapped label laid out shorter than the height its own text needs at its own
// width IS clipped, and that is measurable without looking at a screenshot.
TEST_F(SettingsConfig1Test, TheTrustTabRendersWholeAtTheDialogMinimumSize)
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
    auto* tabs = dlg.findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);
    dlg.show();
    tabs->setCurrentIndex(2);
    dlg.resize(dlg.minimumSize());
    QCoreApplication::processEvents();

    const QList<QLabel*> labels = tabs->currentWidget()->findChildren<QLabel*>();
    ASSERT_FALSE(labels.isEmpty());
    for (QLabel* label : labels) {
        if (!label->isVisible() || label->text().isEmpty() || !label->wordWrap())
            continue;
        EXPECT_GE(label->height(), label->heightForWidth(label->width()))
            << qPrintable(label->objectName()) << " is cut off at the dialog's own minimum size";
    }
}

// Where the next list comes from is not news that stops being useful once a
// first one is installed: anchors are replaced by importing a newer list, so
// the address has to stand in every state the summary can be in.
TEST_F(SettingsConfig1Test, TheDownloadAddressStandsWhetherOrNotAnchorsAreInstalled)
{
    FakeAgentGateway empty;
    empty.setPresence(librecelik::agent::PresenceState::Ready);
    empty.config[QStringLiteral("CscaAnchorState")] = QVariantMap();
    SettingsDialog emptyDlg(&empty);

    FakeAgentGateway held;
    held.setPresence(librecelik::agent::PresenceState::Ready);
    QVariantMap state;
    state[QStringLiteral("anchors")] = QVariant::fromValue<quint32>(412);
    state[QStringLiteral("issuers")] = QVariant::fromValue<quint32>(78);
    state[QStringLiteral("replayRefusalActive")] = true;
    held.config[QStringLiteral("CscaAnchorState")] = state;
    SettingsDialog heldDlg(&held);

    for (SettingsDialog* dlg : {&emptyDlg, &heldDlg}) {
        auto* help = dlg->findChild<QLabel*>(QStringLiteral("cscaHelpLabel"));
        ASSERT_NE(help, nullptr);
        EXPECT_FALSE(help->isHidden());
        EXPECT_TRUE(help->text().contains(QStringLiteral("pkddownload.icao.int"))) << qPrintable(help->text());
    }
    // And the two summaries still say different things -- the standing advice
    // did not flatten the state it sits under.
    EXPECT_EQ(cscaSummaryText(emptyDlg), qtTrId("lc-settings-csca-state-none"));
    EXPECT_TRUE(cscaSummaryText(heldDlg).contains(QStringLiteral("412")));
}
