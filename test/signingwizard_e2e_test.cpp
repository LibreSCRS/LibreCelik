// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The signing wizard end-to-end against a REAL agent and a REAL card.
///
/// This suite has LEFT CI. Every case here dials a live agent, drives the real
/// wizard, and spends a real signing ceremony on a real token, so it belongs to
/// the hardware gate and to nothing else. Everywhere else it builds, is
/// discovered, and skips.
///
/// ## How to run it (hardware bench only)
///
/// The suite is a client of the acceptance harness in `LibreLinux/e2e/`:
/// `launch-e2e.sh` starts a PRIVATE session bus, brings the agent and the
/// headless `auto-prompter` up on it, and prints the bus address. The operator
/// exports that address and runs this binary against it:
///
/// ```
///   ./launch-e2e.sh                      # prints DBUS_SESSION_BUS_ADDRESS
///   export DBUS_SESSION_BUS_ADDRESS=...  # the PRIVATE bus, never the desktop one
///   export LIBRESCRS_HW=1
///   export LIBRESCRS_READER_SUBSTR='<a substring of the bench reader name>'
///   ctest --test-dir build/test --no-tests=error -R SigningWizardE2E
/// ```
///
/// The PRIVATE bus is not a convenience, it is the safety property. The
/// auto-prompter answers ANY `Prompter1.RequestSecret` on its bus and has no
/// reader or card targeting of its own; on the desktop session bus the ordinary
/// prompter may own `Prompter1` instead and invite a hand-typed secret. On the
/// harness bus the auto-prompter is the SOLE `Prompter1` owner by construction,
/// and it answers from `LIBRESCRS_TEST_PIN`, which the harness propagates into
/// the bus environment. LC never reads that variable, never renders a secret
/// field, and never carries a secret in-process: the agent's prompter is the
/// only collector.
///
/// Because the prompter cannot target a reader, the BENCH does: the suite
/// refuses to run unless exactly ONE carded reader is present, and that reader
/// matches `LIBRESCRS_READER_SUBSTR`. Physically remove every other token
/// first. On the first failed ceremony the whole remaining suite skips — three
/// failed verifications block a credential permanently, and this suite never
/// walks toward that.

#include "agent/cardcontroller.h"
#include "agent/live/liveagentgateway.h"
#include "agent/signcontroller.h"
#include "fake_gateway/controllercontract.h"

#include "signing/filedropzone.h"
#include "signing/fileselectionpage.h"
#include "signing/signingwizard.h"
#include "signing/signpage.h"

#include <LibreSCRS/AgentClient/SignOptions.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

using librecelik::test::agent::ReadContractExpectation;
using librecelik::test::agent::SignContractExpectation;
using LibreSCRS::AgentClient::CertificateInfo;

// ---------------------------------------------------------------------------
// Global latch: set when a signing ceremony fails (a possible wrong secret).
// Every remaining case then skips, so the suite can never spend a second and
// third verification attempt and block the credential permanently.
// ---------------------------------------------------------------------------
static bool g_pinFailed = false;

#define SKIP_IF_PIN_FAILED()                                                                                           \
    do {                                                                                                               \
        if (g_pinFailed)                                                                                               \
            GTEST_SKIP() << "Skipped: a previous ceremony failed (possible credential issue)";                         \
    } while (0)

/// A live ceremony waits on a human-paced prompter and then on the agent's own
/// signing pipeline. Generous on purpose: an expiry here would be a false red.
static constexpr int kLiveSignWaitMs = 180000;
/// A full identity read, photo included, off a contactless token.
static constexpr int kLiveReadWaitMs = 120000;
/// The certificate roster arrives without any consent step.
static constexpr int kCertificateWaitMs = 60000;

// Minimal valid PDF with correct xref byte offsets
static QByteArray buildTestPdf()
{
    std::string obj1 = "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n";
    std::string obj2 = "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n";
    std::string obj3 = "3 0 obj<</Type/Page/MediaBox[0 0 612 792]/Parent 2 0 R>>endobj\n";
    std::string header = "%PDF-1.4\n";
    size_t off1 = header.size();
    size_t off2 = off1 + obj1.size();
    size_t off3 = off2 + obj2.size();
    size_t xrefOff = off3 + obj3.size();

    char xref[512];
    snprintf(xref, sizeof(xref),
             "xref\n0 4\n"
             "0000000000 65535 f \n"
             "%010zu 00000 n \n"
             "%010zu 00000 n \n"
             "%010zu 00000 n \n"
             "trailer<</Size 4/Root 1 0 R>>\n"
             "startxref\n%zu\n%%%%EOF",
             off1, off2, off3, xrefOff);

    std::string content = header + obj1 + obj2 + obj3 + xref;
    return QByteArray(content.data(), static_cast<int>(content.size()));
}

static QByteArray buildTestXml()
{
    return QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<Invoice xmlns=\"urn:oasis:names:specification:ubl:schema:xsd:Invoice-2\">\n"
                      "  <ID>INV-001</ID>\n"
                      "  <IssueDate>2026-04-03</IssueDate>\n"
                      "  <InvoiceTypeCode>380</InvoiceTypeCode>\n"
                      "</Invoice>\n");
}

class SigningWizardE2ETest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "SigningWizardE2ETests";
            static char* argv[] = {arg0, nullptr};
            app = new QApplication(argc, argv);
        }

        // Install the LC English translator so qtTrId() returns the source
        // text (with %1/%2 placeholders) instead of the bare ID. Without
        // this, signpage strings like `qtTrId("lc-sign-ok").arg(...)` warn
        // about unfilled args because qtTrId falls back to the literal ID
        // which has no placeholders.
        // qt_add_translations binds the :/i18n/ resource only to the main
        // LibreCelik target, so the test binary has no embedded .qm. Load
        // the build-output .qm from the filesystem instead.
        if (!translator) {
            translator = new QTranslator(app);
            const QString qmPath = QStringLiteral(LIBRECELIK_QM_DIR);
            if (translator->load(QStringLiteral("LibreCelik_en"), qmPath))
                QApplication::installTranslator(translator);
        }

        // --- hardware opt-in ---
        // Nothing below this line is safe to run unattended on a developer
        // machine: it dials an agent and spends a credential.
        const char* hw = std::getenv("LIBRESCRS_HW");
        if (hw == nullptr || std::string(hw) != "1") {
            suiteSkipReason = "LIBRESCRS_HW=1 is not set: the live-agent legs run on the hardware bench only";
            return;
        }

        // --- reader targeting (mandatory) ---
        // The auto-prompter answers every request on its bus with no reader or
        // card targeting, so the operator has to name the bench reader and the
        // suite has to agree that the one card it can see IS that reader's.
        const char* readerSubstr = std::getenv("LIBRESCRS_READER_SUBSTR");
        if (readerSubstr == nullptr || *readerSubstr == '\0') {
            suiteSkipReason = "LIBRESCRS_READER_SUBSTR is not set: the bench reader must be named explicitly";
            return;
        }
        const QString readerNeedle = QString::fromUtf8(readerSubstr);

        // (There is deliberately NO external-oracle gate here. The suite's
        // oracle is its own artifact-shape assertions — the Task-37 review
        // established that no external verification invocation ever existed
        // in this suite — and the old LIBRESCRS_DSS_JAR gate demanded a jar
        // NOTHING here runs, skipping the whole suite on a healthy bench:
        // the Leg-10 bench catch, 2026-08-17.)

        // --- agent presence ---
        gateway = std::make_unique<librecelik::agent::LiveAgentGateway>();
        if (gateway->presence() != librecelik::agent::PresenceState::Ready) {
            suiteSkipReason = "No agent is available on this bus (presence is not Ready)";
            return;
        }

        // --- single-card discipline ---
        // Two carded readers mean the prompter could authorise a ceremony on
        // the token the operator did NOT mean to spend. Say so loudly rather
        // than picking one.
        QList<librecelik::agent::ReaderInfo> carded;
        for (const librecelik::agent::ReaderInfo& reader : gateway->readers()) {
            if (reader.hasCard && !reader.cardId.isEmpty())
                carded.append(reader);
        }
        if (carded.isEmpty()) {
            suiteSkipReason = "No carded reader on this bench";
            return;
        }
        if (carded.size() > 1) {
            suiteSkipReason = "SINGLE-CARD DISCIPLINE: " + std::to_string(carded.size()) +
                              " carded readers are present. The auto-prompter answers every request on this bus and "
                              "cannot target a reader, so physically remove every other token and re-run.";
            return;
        }
        const librecelik::agent::ReaderInfo& target = carded.constFirst();
        if (!target.id.contains(readerNeedle) && !target.name.contains(readerNeedle)) {
            suiteSkipReason =
                "The only carded reader (" + target.name.toStdString() + ") does not match LIBRESCRS_READER_SUBSTR";
            return;
        }
        cardId = target.cardId;

        // --- The certificate the whole suite signs with ---
        auto* controller = gateway->cardController(cardId);
        if (controller == nullptr) {
            suiteSkipReason = "The agent has no controller for the card in the bench reader";
            return;
        }
        QList<CertificateInfo> certificates;
        bool answered = false;
        QObject::connect(controller, &librecelik::agent::CardController::certificatesReady,
                         [&](const QList<CertificateInfo>& certs) {
                             certificates = certs;
                             answered = true;
                         });
        QObject::connect(controller, &librecelik::agent::CardController::errorOccurred,
                         [&](const QString&) { answered = true; });
        controller->requestCertificates();
        librecelik::test::agent::waitFor([&] { return answered; }, kCertificateWaitMs);

        for (const CertificateInfo& cert : certificates) {
            // A VALID signing certificate, not merely a signing-capable one:
            // an expired certificate at the baseline level opens a modal
            // consent dialog, and a modal exec() in an unattended offscreen run
            // never returns.
            const QDateTime now = QDateTime::currentDateTimeUtc();
            if (cert.signingCapable && cert.notAfter > now) {
                certificate = cert;
                break;
            }
        }
        if (certificate.id.isEmpty()) {
            suiteSkipReason = "The card carries no valid signing certificate";
            return;
        }
    }

    static void TearDownTestSuite()
    {
        // The controllers are parented to the gateway and go with it.
        gateway.reset();
    }

    void SetUp() override
    {
        SKIP_IF_PIN_FAILED();
        if (!suiteSkipReason.empty())
            GTEST_SKIP() << suiteSkipReason;
    }

    // ---- fixture helpers ---------------------------------------------------

    static void writeFile(const QString& path, const QByteArray& bytes)
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly)) << "could not write " << path.toStdString();
        file.write(bytes);
        file.close();
    }

    /// The page that is on screen right now. The wizard names its pages, and
    /// the walk below drives by that name rather than by a fixed step count:
    /// the placement page appears only for a PDF the agent can both stamp and
    /// lay out, which is the agent's answer, not this suite's.
    [[nodiscard]] static QString currentPageObjectName(const SigningWizard& wizard)
    {
        auto* stack = wizard.findChild<QStackedWidget*>();
        if (stack == nullptr || stack->currentWidget() == nullptr)
            return {};
        return stack->currentWidget()->objectName();
    }

    /// The signature-level combo, identified by the level tokens it carries —
    /// the selection page stacks a second combo for the timestamp authority,
    /// and a positional lookup would silently start driving that one.
    [[nodiscard]] static QComboBox* levelCombo(const SigningWizard& wizard)
    {
        for (QComboBox* combo : wizard.findChildren<QComboBox*>()) {
            if (combo->findData(QStringLiteral("B_LTA")) >= 0)
                return combo;
        }
        return nullptr;
    }

    /// Latch on a ceremony that produced nothing but failures. Reads the run's
    /// own tally, so a run that partially succeeded (the credential was
    /// accepted, a document was refused for its own reasons) does not stop the
    /// suite.
    static void latchOnFailedCeremony(int succeeded, int failed)
    {
        if (failed > 0 && succeeded == 0) {
            g_pinFailed = true;
            std::cerr << "\n*** SIGNING FAILED (succeeded=0, failed=" << failed
                      << "). Aborting subsequent cases to protect the credential's retry counter. ***\n"
                      << std::endl;
        }
    }

    // Level indices: 0=B-B, 1=B-T, 2=B-LT, 3=B-LTA
    std::pair<int, int> runWizardFlow(const QStringList& filePaths, int levelIdx = 0)
    {
        // Clear persisted output folder so FileSelectionPage derives it from the input
        // file directory (QTemporaryDir) instead of using the user's saved default.
        {
            QSettings settings(QStringLiteral("LibreSCRS"), QStringLiteral("LibreCelik"));
            settings.remove(QStringLiteral("signing/defaultOutputFolder"));
        }

        // The wizard owns no card, no session and no secret: the gateway hands
        // it the card's sign controller and the agent's prompter collects
        // every credential. There is nothing for this suite to type.
        SigningWizard wizard(certificate, cardId, gateway.get());

        wizard.show();
        QApplication::processEvents();

        // --- Page 0: File Selection ---
        auto* dropZone = wizard.findChild<FileDropZone*>();
        if (!dropZone) {
            ADD_FAILURE() << "FileDropZone not found";
            return {0, 1};
        }
        dropZone->addFiles(filePaths);
        QApplication::processEvents();

        auto* combo = levelCombo(wizard);
        if (!combo) {
            ADD_FAILURE() << "signature level combo not found";
            return {0, 1};
        }
        combo->setCurrentIndex(levelIdx);
        QApplication::processEvents();

        auto* nextBtn = wizard.findChild<QPushButton*>(QStringLiteral("nextBtn"));
        if (!nextBtn) {
            ADD_FAILURE() << "nextBtn not found";
            return {0, 1};
        }
        EXPECT_TRUE(nextBtn->isEnabled());

        // --- Walk to the sign page ---
        for (int guard = 0; guard < 3 && currentPageObjectName(wizard) != QStringLiteral("signPage"); ++guard) {
            QTest::mouseClick(nextBtn, Qt::LeftButton);
            QApplication::processEvents();
        }
        if (currentPageObjectName(wizard) != QStringLiteral("signPage")) {
            ADD_FAILURE() << "the wizard never reached the sign page";
            return {0, 1};
        }

        auto* signPageWidget = wizard.findChild<SignPage*>();
        if (!signPageWidget) {
            ADD_FAILURE() << "SignPage not found";
            return {0, 1};
        }

        EXPECT_TRUE(nextBtn->isEnabled());
        QSignalSpy spy(signPageWidget, &SignPage::signingFinished);

        // --- Sign: the prompter answers, not this process ---
        QTest::mouseClick(nextBtn, Qt::LeftButton);

        EXPECT_TRUE(spy.wait(kLiveSignWaitMs)) << "signingFinished not emitted within " << kLiveSignWaitMs << " ms";
        if (spy.isEmpty())
            return {0, 1};

        int succeeded = spy.at(0).at(0).toInt();
        int failed = spy.at(0).at(1).toInt();
        latchOnFailedCeremony(succeeded, failed);
        return {succeeded, failed};
    }

    static QApplication* app;
    static QTranslator* translator;
    static std::unique_ptr<librecelik::agent::LiveAgentGateway> gateway;
    static QString cardId;
    static CertificateInfo certificate;
    static std::string suiteSkipReason;
};

QApplication* SigningWizardE2ETest::app = nullptr;
QTranslator* SigningWizardE2ETest::translator = nullptr;
std::unique_ptr<librecelik::agent::LiveAgentGateway> SigningWizardE2ETest::gateway;
QString SigningWizardE2ETest::cardId;
CertificateInfo SigningWizardE2ETest::certificate;
std::string SigningWizardE2ETest::suiteSkipReason;

// ============================================================================
// B-B level tests — whatever card the bench holds
// ============================================================================

TEST_F(SigningWizardE2ETest, SignPdf_BB)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString pdfPath = tmpDir.filePath("test.pdf");
    writeFile(pdfPath, buildTestPdf());

    auto [succeeded, failed] = runWizardFlow({pdfPath});
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    // Verify signed PDF exists and starts with %PDF
    QDir outDir(QFileInfo(pdfPath).absolutePath());
    QString signedPath = outDir.filePath("test-signed.pdf");
    ASSERT_TRUE(QFile::exists(signedPath)) << "Signed PDF not found: " << signedPath.toStdString();
    QFile signed_(signedPath);
    ASSERT_TRUE(signed_.open(QIODevice::ReadOnly));
    QByteArray header = signed_.read(4);
    EXPECT_EQ(header, QByteArray("%PDF"));
}

TEST_F(SigningWizardE2ETest, SignText_BB)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString txtPath = tmpDir.filePath("test.txt");
    writeFile(txtPath, QByteArray("Test document for ASiC-E signing."));

    auto [succeeded, failed] = runWizardFlow({txtPath});
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    // Text files route to an ASiC-E container (.asice)
    QDir outDir(QFileInfo(txtPath).absolutePath());
    QString asicePath = outDir.filePath("test.asice");
    ASSERT_TRUE(QFile::exists(asicePath)) << "ASiC-E container not found: " << asicePath.toStdString();
    QFileInfo fi(asicePath);
    EXPECT_GT(fi.size(), 0);
}

// --- Batch signing tests (2 PDFs + 1 TXT in single wizard run) ---

TEST_F(SigningWizardE2ETest, SignBatch_BB)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    auto pdf = buildTestPdf();
    for (const auto* name : {"doc1.pdf", "doc2.pdf"})
        writeFile(tmpDir.filePath(QString::fromLatin1(name)), pdf);
    writeFile(tmpDir.filePath("notes.txt"), QByteArray("Batch test document."));

    auto [succeeded, failed] =
        runWizardFlow({tmpDir.filePath("doc1.pdf"), tmpDir.filePath("doc2.pdf"), tmpDir.filePath("notes.txt")});
    EXPECT_EQ(succeeded, 3);
    EXPECT_EQ(failed, 0);

    QDir out(tmpDir.path());
    EXPECT_TRUE(QFile::exists(out.filePath("doc1-signed.pdf")));
    EXPECT_TRUE(QFile::exists(out.filePath("doc2-signed.pdf")));
    EXPECT_TRUE(QFile::exists(out.filePath("notes.asice")));
}

// --- ASiC-E container tests (non-PDF, non-XML files -> .asice) ---

TEST_F(SigningWizardE2ETest, SignDocx_ASiCE_BB)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString docxPath = tmpDir.filePath("report.docx");
    writeFile(docxPath, QByteArray("PK\x03\x04 mock OOXML content"));

    auto [succeeded, failed] = runWizardFlow({docxPath});
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    // .docx routes to ASiC-E -> output is report.asice
    QDir outDir(tmpDir.path());
    QString asicePath = outDir.filePath("report.asice");
    ASSERT_TRUE(QFile::exists(asicePath)) << "ASiC-E container not found: " << asicePath.toStdString();
    QFileInfo fi(asicePath);
    EXPECT_GT(fi.size(), 0);

    // ASiC-E is a ZIP — verify it starts with PK magic bytes
    QFile asice(asicePath);
    ASSERT_TRUE(asice.open(QIODevice::ReadOnly));
    QByteArray magic = asice.read(2);
    EXPECT_EQ(magic, QByteArray("PK"));
}

// --- XAdES tests (XML files -> enveloped XAdES) ---

TEST_F(SigningWizardE2ETest, SignXml_XAdES_BB)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString xmlPath = tmpDir.filePath("invoice.xml");
    writeFile(xmlPath, buildTestXml());

    auto [succeeded, failed] = runWizardFlow({xmlPath});
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    // XML routes to XAdES enveloped -> output is invoice-signed.xml
    QDir outDir(tmpDir.path());
    QString signedPath = outDir.filePath("invoice-signed.xml");
    ASSERT_TRUE(QFile::exists(signedPath)) << "XAdES signed XML not found: " << signedPath.toStdString();

    // Verify the signed XML contains a ds:Signature element
    QFile signedFile(signedPath);
    ASSERT_TRUE(signedFile.open(QIODevice::ReadOnly));
    QByteArray content = signedFile.readAll();
    EXPECT_TRUE(content.contains("ds:Signature")) << "XAdES enveloped signature element not found";
    EXPECT_TRUE(content.contains("INV-001"));
}

// --- Mixed batch: PDF + XML + DOCX in a single wizard run ---

TEST_F(SigningWizardE2ETest, SignMixedBatch_BB)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    writeFile(tmpDir.filePath("contract.pdf"), buildTestPdf());
    writeFile(tmpDir.filePath("invoice.xml"), buildTestXml());
    writeFile(tmpDir.filePath("report.docx"), QByteArray("PK\x03\x04 mock OOXML content"));

    auto [succeeded, failed] = runWizardFlow(
        {tmpDir.filePath("contract.pdf"), tmpDir.filePath("invoice.xml"), tmpDir.filePath("report.docx")});
    EXPECT_EQ(succeeded, 3);
    EXPECT_EQ(failed, 0);

    QDir out(tmpDir.path());
    EXPECT_TRUE(QFile::exists(out.filePath("contract-signed.pdf")));
    EXPECT_TRUE(QFile::exists(out.filePath("invoice-signed.xml")));
    EXPECT_TRUE(QFile::exists(out.filePath("report.asice")));
}

// --- JAdES tests (JSON files -> .jose detached signature) ---

TEST_F(SigningWizardE2ETest, SignJson_JAdES_BB)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString jsonPath = tmpDir.filePath("data.json");
    writeFile(jsonPath, QByteArray(R"({"name":"test","value":42})"));

    auto [succeeded, failed] = runWizardFlow({jsonPath});
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    // JSON routes to JAdES -> output is data.json.jose
    QDir outDir(tmpDir.path());
    QString josePath = outDir.filePath("data.json.jose");
    ASSERT_TRUE(QFile::exists(josePath)) << "JAdES signature not found: " << josePath.toStdString();
    QFileInfo fi(josePath);
    EXPECT_GT(fi.size(), 0);
}

// ============================================================================
// B-LTA level tests (the agent's own trust configuration + TSA)
// The card only signs the hash; timestamps, revocation data and archive
// timestamps are the agent's business.
// ============================================================================

TEST_F(SigningWizardE2ETest, SignPdf_BLTA)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString pdfPath = tmpDir.filePath("test.pdf");
    writeFile(pdfPath, buildTestPdf());

    auto [succeeded, failed] = runWizardFlow({pdfPath}, 3); // B-LTA
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    QDir outDir(tmpDir.path());
    EXPECT_TRUE(QFile::exists(outDir.filePath("test-signed.pdf")));
}

TEST_F(SigningWizardE2ETest, SignText_BLTA)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString txtPath = tmpDir.filePath("test.txt");
    writeFile(txtPath, QByteArray("Test ASiC-E B-LTA signing."));

    auto [succeeded, failed] = runWizardFlow({txtPath}, 3); // B-LTA
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    QDir outDir(tmpDir.path());
    EXPECT_TRUE(QFile::exists(outDir.filePath("test.asice")));
}

TEST_F(SigningWizardE2ETest, SignMixedBatch_BLTA)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    writeFile(tmpDir.filePath("contract.pdf"), buildTestPdf());
    writeFile(tmpDir.filePath("invoice.xml"), buildTestXml());
    writeFile(tmpDir.filePath("report.docx"), QByteArray("PK\x03\x04 mock OOXML content"));

    auto [succeeded, failed] = runWizardFlow(
        {tmpDir.filePath("contract.pdf"), tmpDir.filePath("invoice.xml"), tmpDir.filePath("report.docx")}, 3);
    EXPECT_EQ(succeeded, 3);
    EXPECT_EQ(failed, 0);

    QDir out(tmpDir.path());
    EXPECT_TRUE(QFile::exists(out.filePath("contract-signed.pdf")));
    EXPECT_TRUE(QFile::exists(out.filePath("invoice-signed.xml")));
    EXPECT_TRUE(QFile::exists(out.filePath("report.asice")));
}

// ============================================================================
// Live emission contract — the SAME two helpers the scripted fakes are
// asserted against (`fake_gateway/controllercontract.h`). Fake and live are
// pinned to ONE emission contract on BOTH seams, so an offscreen GUI suite can
// never pass on an ordering the real agent does not produce.
// ============================================================================

TEST_F(SigningWizardE2ETest, LiveReadSatisfiesTheSharedEmissionContract)
{
    auto* controller = gateway->cardController(cardId);
    ASSERT_NE(controller, nullptr);

    // The success side of the read contract needs a card that CAN be read:
    // the agent refuses ReadIdentity outright without the IdentityData
    // capability, and a PKI-only signing token (the usual bench card for
    // this suite) would fail the success assertion for a refusal that is
    // correct behavior. Single-card discipline forbids seating a second,
    // identity-capable card alongside — so this case skips honestly instead.
    namespace Cap = LibreSCRS::AgentClient::Cap;
    if (!LibreSCRS::AgentClient::has(controller->capabilityBits(), Cap::IdentityData))
        GTEST_SKIP() << "The bench card carries no IdentityData: the live read contract needs an identity-capable card";

    // No expected script: what this card streams is the card's business. The
    // ordering and count invariants are the contract.
    ReadContractExpectation expectation;
    expectation.waitMs = kLiveReadWaitMs;
    librecelik::test::agent::assertReadEmissionContract(*controller, expectation);
}

TEST_F(SigningWizardE2ETest, LiveSignSatisfiesTheSharedEmissionContract)
{
    auto* controller = gateway->signController(cardId);
    ASSERT_NE(controller, nullptr);

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    const QString pdfPath = tmpDir.filePath("contract.pdf");
    writeFile(pdfPath, buildTestPdf());

    // The baseline level explicitly: `Auto` lets the agent resolve to a
    // timestamped level, which drags a timestamp authority into a case that is
    // about emission ordering.
    LibreSCRS::AgentClient::SignOptions options;
    options.level = LibreSCRS::AgentClient::SignatureLevel::BB;

    // Observed independently of the helper's own connections, so the latch can
    // still read the tally the run reported.
    QSignalSpy finishedSpy(controller, &librecelik::agent::SignController::finished);

    SignContractExpectation expectation;
    expectation.waitMs = kLiveSignWaitMs;
    librecelik::test::agent::assertSignEmissionContract(
        *controller, certificate.id,
        {librecelik::agent::SignRequestItem{pdfPath, LibreSCRS::AgentClient::SignatureFormat::PAdES,
                                            LibreSCRS::AgentClient::Packaging::Enveloped}},
        options, tmpDir.path(), expectation);

    if (!finishedSpy.isEmpty())
        latchOnFailedCeremony(finishedSpy.at(0).at(0).toInt(), finishedSpy.at(0).at(1).toInt());
}
