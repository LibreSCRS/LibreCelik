// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Offscreen behavior coverage for the signing wizard, driven by the
///        scripted sign controller.
///
/// Every case here is about what the wizard DOES with what an agent answers:
/// one result row per document, a halt that marks the documents it was never
/// asked about, the phase line the agent's own vocabulary supplies, the
/// affordances that disappear when the agent cannot back them, the selection
/// cap, the cancel a rejected dialog owes an in-flight run, the consent count
/// a mixed selection really costs, the honest sequential degrade, and the
/// modal hygiene a card removal enforces.
///
/// Nothing here dials anything: the gateway and the sign controller are the
/// campaign's scripted fakes, so the assertions are about LC's own behavior
/// and never about an agent's availability.

#include "agent/agentgateway.h"
#include "agent/errortext.h"
#include "agent/signcontroller.h"
#include "fake_gateway/fakeagentgateway.h"
#include "fake_gateway/fakesigncontroller.h"
#include "settings/settingskeys.h"
#include "signing/filedropzone.h"
#include "signing/fileselectionpage.h"
#include "signing/resultdelegate.h"
#include "signing/signingcolors.h"
#include "signing/signingwizard.h"
#include "signing/signpage.h"

#include <LibreSCRS/AgentClient/CallError.h>
#include <LibreSCRS/AgentClient/ErrorCode.h>
#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <LibreSCRS/AgentClient/SignOptions.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QIODevice>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTranslator>
#include <QVariant>
#include <QWidget>

#include <gtest/gtest.h>

#include <memory>

namespace {

using librecelik::agent::SignRowResult;
using librecelik::test::agent::FakeAgentGateway;
using librecelik::test::agent::FakeSignController;
using LibreSCRS::AgentClient::CallError;
using LibreSCRS::AgentClient::CertificateInfo;
using LibreSCRS::AgentClient::ErrorCode;
using LibreSCRS::AgentClient::OperationPhase;

const QString kCardId = QStringLiteral("card-1");

/// One rendered results-list entry, read back the way the delegate draws it.
/// `ok` is the production success colour, not a re-spelled marker glyph.
struct ResultRow
{
    bool ok = false;
    QString message;
};

} // namespace

/// The wizard under an offscreen application, one Ready card, and a scripted
/// controller for it.
///
/// QApplication is created once for the whole binary and never destroyed:
/// `wizard.show()` without an application object aborts the process under the
/// offscreen platform, and the target's `gtest_main` provides none.
class WizardFake : public ::testing::Test
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
        installTranslatorOnce();
        pinSettingsToATempTree();

        ASSERT_TRUE(temp.isValid());

        gateway_ = std::make_unique<FakeAgentGateway>();
        gateway_->scriptedPresence = librecelik::agent::PresenceState::Ready;
        gateway_->scriptedReaders = {
            librecelik::agent::ReaderInfo{QStringLiteral("reader-1"), QStringLiteral("Reader One"), true, kCardId}};
        gateway_->scriptedFeatures = QStringList{QStringLiteral("batch-sign"), QStringLiteral("visual-sign"),
                                                 QStringLiteral("layout-preview"), QStringLiteral("tsa-url")};
        scriptAgentConfig();

        controller = std::make_unique<FakeSignController>();
        fakeSign = controller.get();
        gateway_->registerSignController(kCardId, fakeSign);

        certificate.id = QStringLiteral("cert-1");
        certificate.subject = QStringLiteral("Scripted Holder");
        certificate.issuer = QStringLiteral("Scripted Issuer");
        certificate.signingCapable = true;
        // A live certificate: an expired one at the baseline level opens a
        // modal consent dialog, and a modal exec() offscreen never returns.
        certificate.notBefore = QDateTime::currentDateTimeUtc().addYears(-1);
        certificate.notAfter = QDateTime::currentDateTimeUtc().addYears(1);

        openWizard();
    }

    /// Removed again once every case in this suite has run, so it cannot
    /// colour another suite sharing this binary.
    static void TearDownTestSuite()
    {
        QCoreApplication::removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    // ---- fixture helpers ----------------------------------------------------

    [[nodiscard]] SigningWizard* wizard() const
    {
        return wiz.get();
    }
    [[nodiscard]] FakeAgentGateway* gateway() const
    {
        return gateway_.get();
    }

    /// Build (or rebuild) the wizard. The page capabilities are read from the
    /// controller in the wizard's constructor, so a scripted capability change
    /// only reaches the pages through a fresh dialog.
    void openWizard()
    {
        finishedSpy.reset();
        wiz.reset();
        wiz = std::make_unique<SigningWizard>(certificate, kCardId, gateway_.get());
        wiz->show();
    }

    [[nodiscard]] QStackedWidget* pageStack() const
    {
        return wiz->findChild<QStackedWidget*>();
    }

    void clickNext() const
    {
        nextButton()->click();
    }

    [[nodiscard]] QString currentPageObjectName() const
    {
        auto* stack = pageStack();
        if (stack == nullptr || stack->currentWidget() == nullptr)
            return {};
        return stack->currentWidget()->objectName();
    }

    /// Walk forward to the sign page WITHOUT pressing Sign. The number of
    /// steps depends on what the agent can do (a placement page appears only
    /// for a visible signature it can both stamp and lay out), so this drives
    /// by the page that is actually on screen rather than by a fixed count.
    void driveToSignPage() const
    {
        for (int guard = 0; guard < 3 && currentPageObjectName() != QStringLiteral("signPage"); ++guard)
            clickNext();
        ASSERT_EQ(currentPageObjectName(), QStringLiteral("signPage"));
    }

    /// Navigate to the sign page and press Sign, with the finished spy armed
    /// first so a synchronous script cannot outrun it.
    void driveToSignPageAndSign()
    {
        driveToSignPage();
        auto* page = signPage();
        ASSERT_NE(page, nullptr);
        finishedSpy = std::make_unique<QSignalSpy>(page, &SignPage::signingFinished);
        clickNext();
    }

    void addFiles(const QStringList& paths) const
    {
        auto* zone = filePage()->findChild<FileDropZone*>();
        ASSERT_NE(zone, nullptr);
        zone->addFiles(paths);
    }

    /// What the selection page will hand the controller. The list widget is
    /// a rendering of exactly this, and the cap governs this.
    [[nodiscard]] int fileListCount() const
    {
        return static_cast<int>(filePage()->selectedFiles().count());
    }

    [[nodiscard]] QList<ResultRow> resultRows() const
    {
        QList<ResultRow> rows;
        auto* list = wiz->findChild<QListWidget*>(QStringLiteral("resultsList"));
        if (list == nullptr)
            return rows;
        for (int index = 0; index < list->count(); ++index) {
            QListWidgetItem* item = list->item(index);
            ResultRow row;
            row.ok = item->data(ResultDelegate::IconColorRole).toString() == QString(signing::kSuccessHex);
            row.message = item->data(ResultDelegate::MessageRole).toString();
            rows.append(row);
        }
        return rows;
    }

    [[nodiscard]] QString consentHintText() const
    {
        return labelText(QStringLiteral("consentHint"));
    }

    /// The selection page's limits area: the cap line and the sequential-
    /// degrade notice share it. Only what is actually ON SCREEN counts — both
    /// labels always carry text and toggle visibility, so reading a hidden
    /// one would make a "the notice is announced" case pass against an agent
    /// that never degrades.
    [[nodiscard]] QString limitsLabelText() const
    {
        QStringList visible;
        for (const QString& name : {QStringLiteral("limitsLabel"), QStringLiteral("sequentialNotice")}) {
            auto* label = wiz->findChild<QLabel*>(name);
            if (label != nullptr && label->isVisibleTo(filePage()))
                visible << label->text();
        }
        return visible.join(QLatin1Char('\n'));
    }

    [[nodiscard]] QString progressLabelText() const
    {
        return labelText(QStringLiteral("progressLabel"));
    }

    [[nodiscard]] QWidget* tsaRowWidget() const
    {
        return wiz->findChild<QWidget*>(QStringLiteral("tsaRow"));
    }

    // ---- row factories ------------------------------------------------------

    [[nodiscard]] static SignRowResult okRow(const QString& path)
    {
        return SignRowResult{path, path + QStringLiteral(".signed"), true, {}};
    }

    [[nodiscard]] static SignRowResult failRow(const QString& path, const QString& message)
    {
        return SignRowResult{path, {}, false, message};
    }

    /// A document the run never reached: it carries the HALTING row's own
    /// message, which is what the live controller propagates (a halt is
    /// inclusive and contagious).
    [[nodiscard]] static SignRowResult haltRow(const QString& path)
    {
        return failRow(path, librecelik::agent::errorText(ErrorCode::CredentialWrong, CallError::None, {}, {}));
    }

    // ---- file factories -----------------------------------------------------

    [[nodiscard]] QString fileA() const
    {
        return mintFile(QStringLiteral("a.pdf"));
    }
    [[nodiscard]] QString fileB() const
    {
        return mintFile(QStringLiteral("b.pdf"));
    }
    [[nodiscard]] QString fileC() const
    {
        return mintFile(QStringLiteral("c.pdf"));
    }
    [[nodiscard]] QString filePdfA() const
    {
        return mintFile(QStringLiteral("mixed-a.pdf"));
    }
    [[nodiscard]] QString fileXmlB() const
    {
        return mintFile(QStringLiteral("mixed-b.xml"));
    }
    [[nodiscard]] QString filePdfC() const
    {
        return mintFile(QStringLiteral("mixed-c.pdf"));
    }

    [[nodiscard]] QStringList thirteenTempPdfs() const
    {
        QStringList paths;
        for (int index = 0; index < 13; ++index)
            paths << mintFile(QStringLiteral("bulk%1.pdf").arg(index));
        return paths;
    }

    FakeSignController* fakeSign = nullptr;
    std::unique_ptr<QSignalSpy> finishedSpy;

private:
    [[nodiscard]] FileSelectionPage* filePage() const
    {
        return wiz->findChild<FileSelectionPage*>(QStringLiteral("filePage"));
    }

    [[nodiscard]] SignPage* signPage() const
    {
        return wiz->findChild<SignPage*>(QStringLiteral("signPage"));
    }

    [[nodiscard]] QPushButton* nextButton() const
    {
        return wiz->findChild<QPushButton*>(QStringLiteral("nextBtn"));
    }

    [[nodiscard]] QString labelText(const QString& objectName) const
    {
        auto* label = wiz->findChild<QLabel*>(objectName);
        return label != nullptr ? label->text() : QString();
    }

    /// Mint (once) a file with @p name in the fixture's temporary directory.
    /// The drop zone refuses anything that is not an existing readable regular
    /// file, and the placement preview opens the PDFs — a header keeps the
    /// bytes at least plausible, and an unloadable document degrades to an
    /// empty preview rather than to a crash.
    [[nodiscard]] QString mintFile(const QString& name) const
    {
        const QString path = temp.filePath(name);
        if (!QFile::exists(path)) {
            QFile file(path);
            EXPECT_TRUE(file.open(QIODevice::WriteOnly)) << "could not mint " << path.toStdString();
            file.write("%PDF-1.4\n");
            file.close();
        }
        return path;
    }

    /// English catalog strings, once per binary. Both plural ids this suite
    /// reads (`lc-sign-consent-hint`, `lc-sign-sequential-notice`) substitute
    /// the count into a TRANSLATED template — untranslated they render the
    /// bare catalog id and the count disappears entirely.
    static void installTranslatorOnce()
    {
        if (translator != nullptr)
            return;
        translator = new QTranslator();
        const QString qmDir = QStringLiteral(LIBRECELIK_TRANSLATIONS_DIR_DEFAULT);
        ASSERT_TRUE(translator->load(QStringLiteral("LibreCelik_en"), qmDir))
            << "failed to load LibreCelik_en.qm from " << qmDir.toStdString();
        QCoreApplication::installTranslator(translator);
    }

    /// Keep the wizard's remaining file-backed state — the default output
    /// folder — off the developer's real configuration AND out of any
    /// directory another suite shares. Everything the AGENT owns (level, TSA
    /// list, placement reason/location) comes from the scripted config
    /// snapshot instead, see @ref scriptAgentConfig.
    static void pinSettingsToATempTree()
    {
        static QTemporaryDir settingsDir;
        if (settingsPinned)
            return;
        ASSERT_TRUE(settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir.path());
        settingsPinned = true;
    }

    /// The default level is deliberately a TIMESTAMPED one: the TSA row is
    /// gated on the level as well as on the capability, so at the baseline
    /// level the row would be hidden whatever the agent can do and the
    /// capability-gating case would prove nothing.
    void scriptAgentConfig() const
    {
        gateway_->config[QStringLiteral("DefaultLevel")] = QStringLiteral("b-t");
    }

    QTemporaryDir temp;
    CertificateInfo certificate;
    std::unique_ptr<FakeAgentGateway> gateway_;
    std::unique_ptr<FakeSignController> controller;
    std::unique_ptr<SigningWizard> wiz;

    static QApplication* app;
    static QTranslator* translator;
    static bool settingsPinned;
};

QApplication* WizardFake::app = nullptr;
QTranslator* WizardFake::translator = nullptr;
bool WizardFake::settingsPinned = false;

TEST_F(WizardFake, HappyBatchRendersOneResultRowPerFile)
{
    fakeSign->scriptedRows = {okRow(fileA()), okRow(fileB())};
    addFiles({fileA(), fileB()});
    driveToSignPageAndSign();
    ASSERT_EQ(resultRows().size(), 2);
    EXPECT_TRUE(resultRows()[0].ok);
    EXPECT_TRUE(resultRows()[1].ok);
    EXPECT_EQ(finishedSpy->takeFirst(), (QList<QVariant>{2, 0}));
}

TEST_F(WizardFake, HaltOnPinFailMarksRemainingRowsHalted)
{
    fakeSign->scriptedRows = {
        okRow(fileA()),
        failRow(fileB(), librecelik::agent::errorText(ErrorCode::CredentialWrong, CallError::None, {}, {})),
        haltRow(fileC())};
    addFiles({fileA(), fileB(), fileC()});
    driveToSignPageAndSign();
    ASSERT_EQ(resultRows().size(), 3);
    EXPECT_TRUE(resultRows()[0].ok);
    EXPECT_FALSE(resultRows()[1].ok);
    EXPECT_FALSE(resultRows()[2].ok);
    EXPECT_EQ(finishedSpy->takeFirst(), (QList<QVariant>{1, 2}));
}

TEST_F(WizardFake, AwaitingConsentPhaseShowsConsentHint)
{
    fakeSign->scriptedPhases = {OperationPhase::AwaitingConsent};
    // Parked at the prompter: the phase is reported and nothing follows it.
    // A run that terminates in the same call stack would overwrite the
    // progress line with its completion tally before anything could read it.
    fakeSign->holdOpen = true;
    addFiles({fileA()});
    driveToSignPageAndSign();
    EXPECT_EQ(progressLabelText(), librecelik::agent::phaseText(OperationPhase::AwaitingConsent));
}

TEST_F(WizardFake, VisualPlacementPageHiddenWithoutVisualSignFeature)
{
    fakeSign->scriptedCanVisual = false;
    addFiles({fileA()}); // a PDF — would show placement when capable
    clickNext();
    EXPECT_EQ(currentPageObjectName(), QStringLiteral("signPage")); // skipped placement
}

TEST_F(WizardFake, TsaRowHiddenWithoutTsaUrlFeature)
{
    fakeSign->scriptedCanTsa = false;
    openWizard();
    ASSERT_NE(tsaRowWidget(), nullptr);
    EXPECT_FALSE(tsaRowWidget()->isVisibleTo(wizard()));
}

TEST_F(WizardFake, ThirteenFilesRefusedAtSelectionPage)
{
    addFiles(thirteenTempPdfs());
    EXPECT_EQ(fileListCount(), 12);
    EXPECT_TRUE(
        limitsLabelText().contains(qtTrId("lc-sign-too-many-files").arg(LibreSCRS::AgentClient::kMaxBatchDocuments)));
}

TEST_F(WizardFake, RejectDuringSigningCancelsTheController)
{
    fakeSign->holdOpen = true; // start() emits nothing until released
    addFiles({fileA()});
    driveToSignPageAndSign();
    // Through QDialog, where `reject()` is the public slot Esc and the button
    // box fire: the wizard's override narrows the STATIC access only, and the
    // call still dispatches to that override.
    static_cast<QDialog*>(wizard())->reject();
    EXPECT_TRUE(fakeSign->cancelCalled);
}

TEST_F(WizardFake, MixedSelectionDisclosesTheRealCeremonyCount)
{
    // (a.pdf, b.xml, c.pdf) → stable-sort partition = TWO runs;
    // the consent hint must say 2, and interleaving must not make it 3.
    addFiles({filePdfA(), fileXmlB(), filePdfC()});
    driveToSignPage();
    EXPECT_TRUE(consentHintText().contains(QStringLiteral("2")));
}

TEST_F(WizardFake, BatchlessAgentDegradesToSequentialWithHonestNotice)
{
    fakeSign->scriptedCanBatch = false;
    openWizard();                          // the pages read the capability in the wizard's constructor
    addFiles({fileA(), fileB(), fileC()}); // multi-file STAYS allowed
    EXPECT_EQ(fileListCount(), 3);
    EXPECT_TRUE(limitsLabelText().contains(qtTrId("lc-sign-sequential-notice", 3)));
    driveToSignPage();
    EXPECT_TRUE(consentHintText().contains(QStringLiteral("3"))); // N per-file prompts
}

TEST_F(WizardFake, CardRemovalClosesTheWizardMidFlow)
{
    // Modal hygiene (§5.4) — the wizard's OWN gateway subscription, no
    // window glue involved: removal (or presence loss, which fans out
    // cardRemoved for every card) rejects the dialog.
    addFiles({fileA()});
    driveToSignPage();
    gateway()->removeCard(kCardId); // fake emits cardRemoved
    EXPECT_EQ(wizard()->result(), QDialog::Rejected);
    EXPECT_FALSE(wizard()->isVisible());
}
