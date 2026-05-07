// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>

#include <signing/signingwizard.h>
#include <signing/fileselectionpage.h>
#include <signing/signatureplacementpage.h>
#include <signing/signpage.h>
#include <signing/filedropzone.h>

#include <LibreSCRS/Plugin/CardPlugin.h>
#include <LibreSCRS/Plugin/CardPluginService.h>
#include <LibreSCRS/Signing/SigningService.h>
#include <LibreSCRS/Signing/TsaProvider.h>
#include <LibreSCRS/Trust/TrustConfig.h>
#include <LibreSCRS/Trust/TrustStoreService.h>
#include <LibreSCRS/SmartCard/CardSession.h>
#include <LibreSCRS/SmartCard/MonitorService.h>

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>

#include <cstdlib>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Global flag: set when signing fails (possible wrong PIN). All subsequent
// tests are skipped to prevent burning PIN retries (3 = permanent block).
// ---------------------------------------------------------------------------
static bool g_pinFailed = false;

#define SKIP_IF_PIN_FAILED()                                                                                           \
    do {                                                                                                               \
        if (g_pinFailed)                                                                                               \
            GTEST_SKIP() << "Skipped: previous signing failed (possible PIN issue)";                                   \
    } while (0)

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

struct ReaderInfo
{
    std::string readerName;
    std::string cardType;
    std::vector<LibreSCRS::Plugin::CertificateData> certificates;
};

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

        // --- PINs (per-card-type, fallback to generic) ---
        // LIBRESCRS_TEST_PIN_PIV, LIBRESCRS_TEST_PIN_PKCS15, LIBRESCRS_TEST_PIN_CARDEDGE
        // LIBRESCRS_TEST_PIN_PKCS15_CL for eMRTD+PKCS#15 on contactless (CAN:PIN format)
        // fall back to LIBRESCRS_TEST_PIN if card-specific not set
        const char* genericPin = std::getenv("LIBRESCRS_TEST_PIN");
        for (const auto& type : {"PIV", "PKCS15", "CARDEDGE", "PKCS15_CL"}) {
            std::string envName = std::string("LIBRESCRS_TEST_PIN_") + type;
            const char* pin = std::getenv(envName.c_str());
            if (pin && std::string(pin).length() > 0)
                pinMap[QString::fromUtf8(type).toLower()] = QString::fromUtf8(pin);
            else if (genericPin && std::string(genericPin).length() > 0)
                pinMap[QString::fromUtf8(type).toLower()] = QString::fromUtf8(genericPin);
        }
        if (pinMap.empty()) {
            suiteSkipReason = "No test PINs set (need LIBRESCRS_TEST_PIN or LIBRESCRS_TEST_PIN_<TYPE>)";
            return;
        }

        // --- DSS JAR ---
        std::string jarPath;
        const char* jarEnv = std::getenv("LIBRESCRS_DSS_JAR");
        if (jarEnv && fs::exists(jarEnv)) {
            jarPath = jarEnv;
        } else {
            jarPath = std::string(DSS_JAR_DIR) + "/dss-service-1.0.0-SNAPSHOT.jar";
        }
        if (!fs::exists(jarPath)) {
            suiteSkipReason = "DSS JAR not found: " + jarPath;
            return;
        }
        if (std::system("java -version > /dev/null 2>&1") != 0) {
            suiteSkipReason = "Java not available";
            return;
        }

        // DSS JAR is retained for the verification oracle only; the signing
        // backend stays on Native (the default when LIBRESCRS_SIGNING_BACKEND
        // is unset).
        setenv("LIBRESCRS_DSS_JAR", jarPath.c_str(), 1);

        // SigningService is constructed once with
        // trust + TSA and is immutable post-ctor. Build the TL sources here
        // so runWizardFlow doesn't need to reconfigure.
        LibreSCRS::Trust::TrustConfig trust;
        trust.trustedListSources.push_back({"https://www.mit.gov.rs/TrustedList/TSL-RS.xml", false, true});
        trust.trustedListSources.push_back({"https://ec.europa.eu/tools/lotl/eu-lotl.xml", true, false});
        auto tslCacheDir = std::filesystem::path(
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString() + "/tsl");
        // TrustStoreService::create() rejects a non-existent cacheDirectory
        // with InvalidConfig, so materialise it before passing the path in.
        // (On a fresh macOS test run the cache root has never been used and
        // the directory is missing; Linux CI happens to inherit it from
        // earlier runs.)
        std::error_code ec;
        std::filesystem::create_directories(tslCacheDir, ec);
        trust.cacheDirectory = std::move(tslCacheDir);

        // TSA is required for B-T / B-LT / B-LTA levels; configure a public
        // one on the shared service. Override via LIBRESCRS_TEST_TSA_URL when
        // a specific authority is needed. Fallback list matches
        // signing::defaultTsaUrls() so this test mirrors the wizard's default.
        std::string tsaUrl;
        if (const char* envTsa = std::getenv("LIBRESCRS_TEST_TSA_URL"); envTsa && *envTsa)
            tsaUrl = envTsa;
        else
            tsaUrl = "https://timestamp.sectigo.com";

        try {
            auto trustResult = LibreSCRS::Trust::TrustStoreService::create(std::move(trust));
            if (!trustResult) {
                suiteSkipReason = std::string("Failed to create LibreSCRS::Trust::TrustStoreService: ") +
                                  trustResult.error().userMessage.defaultText;
                return;
            }
            sharedTrustService = *trustResult;
            sharedSigningService = std::make_shared<LibreSCRS::Signing::SigningService>(
                sharedTrustService, LibreSCRS::Signing::staticTsa(tsaUrl));
        } catch (const std::exception& e) {
            suiteSkipReason = std::string("Failed to create LibreSCRS::Signing::SigningService: ") + e.what();
            return;
        }

        // --- PKCS#11 module ---
        std::string p11 = std::string(PKCS11_MODULE_PATH);
        if (!fs::exists(p11)) {
            suiteSkipReason = "PKCS#11 module not found: " + p11;
            return;
        }
        // SigningService::detectPkcs11Module() searches exe-relative paths and
        // falls back to bare-name `dlopen`. Apple's dlopen does not pick up
        // the in-tree `build/lib/pkcs11/` location automatically (the test
        // exe lives in `build/test/`, and the relative-path search list
        // doesn't normalise `..` reliably across all macOS versions).
        // Pin the in-tree build artefact via the documented env-var override
        // so the wizard flow uses the freshly-built module.
        setenv("LIBRESCRS_PKCS11_MODULE", p11.c_str(), 1);

        // --- Middleware plugins ---
        sharedPluginRegistry =
            std::make_unique<LibreSCRS::Plugin::CardPluginService>(std::filesystem::path{MIDDLEWARE_PLUGIN_DIR});

        // --- Discover cards in readers ---
        LibreSCRS::SmartCard::MonitorService monitor;
        auto readersOpt = monitor.listReaders();
        if (!readersOpt.has_value() || readersOpt->empty()) {
            suiteSkipReason = "No PC/SC readers found";
            return;
        }
        const auto& readers = *readersOpt;

        for (const auto& readerName : readers) {
            try {
                auto opened = LibreSCRS::SmartCard::CardSession::open(readerName);
                if (!opened.has_value())
                    continue;
                auto session = std::make_shared<LibreSCRS::SmartCard::CardSession>(std::move(*opened));
                auto candidates = sharedPluginRegistry->findAllCandidates(session->atr(), *session);
                if (candidates.empty())
                    continue;

                auto plugin = candidates.front();
                if (!LibreSCRS::Plugin::hasCapability(plugin->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI))
                    continue;

                auto certs = plugin->readCertificates(*session);
                if (certs.empty())
                    continue;

                sharedCards.push_back({readerName, plugin->pluginId(), std::move(certs)});
            } catch (...) {
                // Reader has no card or connection failed — skip it
            }
        }

        // Enable expired cert bypass for testing (debug builds only)
        qputenv("LIBRESCRS_ALLOW_EXPIRED_CERT", "1");
    }

    static void TearDownTestSuite()
    {
        sharedSigningService.reset();
        sharedTrustService.reset();
    }

    void SetUp() override
    {
        SKIP_IF_PIN_FAILED();
        if (!suiteSkipReason.empty())
            GTEST_SKIP() << suiteSkipReason;
    }

    // Return the first available PKI-capable card that has a PIN configured
    static const ReaderInfo* firstCard()
    {
        for (const auto& card : sharedCards) {
            QString key = QString::fromStdString(card.cardType);
            if (pinMap.contains(key))
                return &card;
        }
        return nullptr;
    }

    static bool isCLReader(const std::string& readerName)
    {
        return readerName.find("CL") != std::string::npos || readerName.find("Contactless") != std::string::npos;
    }

    // Level indices: 0=B-B, 1=B-T, 2=B-LT, 3=B-LTA
    std::pair<int, int> runWizardFlow(const ReaderInfo& card, const QStringList& filePaths, bool hasPdf,
                                      int levelIdx = 0)
    {
        // Clear persisted output folder so FileSelectionPage derives it from the input
        // file directory (QTemporaryDir) instead of using the user's saved default.
        {
            QSettings settings(QStringLiteral("LibreSCRS"), QStringLiteral("LibreCelik"));
            settings.remove(QStringLiteral("signing/defaultOutputFolder"));
        }

        // Trust + TSA are baked into sharedSigningService at SetUpTestSuite
        // (see  — SigningService is immutable post-ctor).
        // Nothing to reconfigure per-test.

        auto wizardOpened = LibreSCRS::SmartCard::CardSession::open(card.readerName);
        if (!wizardOpened.has_value()) {
            // Match the pre-v4.0 throw-skip shape: propagate via GTEST_SKIP
            // rather than ADD_FAILURE since the test is hardware-dependent
            // and a removed card in the middle of the suite is not a code
            // defect.
            ADD_FAILURE() << "Cannot open wizard CardSession for " << card.readerName;
            return {-1, -1};
        }
        auto wizardSession = std::make_shared<LibreSCRS::SmartCard::CardSession>(std::move(*wizardOpened));
        auto candidates = sharedPluginRegistry->findAllCandidates(wizardSession->atr(), *wizardSession);
        // CardPluginService hands out shared_ptr<CardPlugin> directly — the
        // SigningWizard shared-ownership contract is satisfied without any
        // no-op-deleter aliasing hack.
        auto wizardPlugin = candidates.front();
        SigningWizard wizard(card.certificates.front(), card.readerName, sharedSigningService, std::move(wizardPlugin),
                             std::move(wizardSession));

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

        // Set signature level (0=B-B, 1=B-T, 2=B-LT, 3=B-LTA)
        auto* levelCombo = wizard.findChild<QComboBox*>();
        if (levelCombo)
            levelCombo->setCurrentIndex(levelIdx);
        QApplication::processEvents();

        auto* nextBtn = wizard.findChild<QPushButton*>(QStringLiteral("nextBtn"));
        if (!nextBtn) {
            ADD_FAILURE() << "nextBtn not found";
            return {0, 1};
        }
        EXPECT_TRUE(nextBtn->isEnabled());
        QTest::mouseClick(nextBtn, Qt::LeftButton);
        QApplication::processEvents();

        // --- Page 1: Signature Placement (PDF only) ---
        auto* stack = wizard.findChild<QStackedWidget*>();
        if (!stack) {
            ADD_FAILURE() << "QStackedWidget not found";
            return {0, 1};
        }
        if (hasPdf) {
            EXPECT_EQ(stack->currentIndex(), 1);
            QTest::mouseClick(nextBtn, Qt::LeftButton);
            QApplication::processEvents();
        }

        // --- Page 2: Sign ---
        EXPECT_EQ(stack->currentIndex(), 2);

        auto* signPageWidget = wizard.findChild<SignPage*>();
        if (!signPageWidget) {
            ADD_FAILURE() << "SignPage not found";
            return {0, 1};
        }

        // Find PIN and CAN fields — PIN has Password echo mode, CAN has Normal
        QLineEdit* pinEdit = nullptr;
        QLineEdit* canEdit = nullptr;
        for (auto* edit : signPageWidget->findChildren<QLineEdit*>()) {
            if (edit->echoMode() == QLineEdit::Password)
                pinEdit = edit;
            else if (!canEdit)
                canEdit = edit;
        }
        if (!pinEdit) {
            ADD_FAILURE() << "PIN QLineEdit not found";
            return {0, 1};
        }

        // Look up credentials — CL readers use "pkcs15_cl" key with CAN:PIN format
        QString pinKey = QString::fromStdString(card.cardType);
        if (isCLReader(card.readerName) && pinMap.contains(pinKey + QStringLiteral("_cl")))
            pinKey += QStringLiteral("_cl");
        auto pinIt = pinMap.find(pinKey);
        if (pinIt == pinMap.end()) {
            ADD_FAILURE() << "No PIN for card: " << pinKey.toStdString();
            return {0, 1};
        }
        QString credentials = pinIt->second;

        if (canEdit && canEdit->isVisible() && credentials.contains(QLatin1Char(':'))) {
            // CAN:PIN format — split and fill both fields
            int colonPos = credentials.indexOf(QLatin1Char(':'));
            canEdit->setText(credentials.left(colonPos));
            pinEdit->setText(credentials.mid(colonPos + 1));
        } else {
            pinEdit->setText(credentials);
        }
        QApplication::processEvents();

        EXPECT_TRUE(nextBtn->isEnabled());
        QSignalSpy spy(signPageWidget, &SignPage::signingFinished);

        QTest::mouseClick(nextBtn, Qt::LeftButton);

        // Wait for signing to complete (DSS + PKCS#11 can take a while)
        EXPECT_TRUE(spy.wait(60000)) << "signingFinished not emitted within 60s";
        if (spy.isEmpty())
            return {0, 1};

        int succeeded = spy.at(0).at(0).toInt();
        int failed = spy.at(0).at(1).toInt();

        // If first signing attempt fails, assume PIN issue and abort all future tests
        if (failed > 0 && succeeded == 0) {
            g_pinFailed = true;
            std::cerr << "\n*** SIGNING FAILED (succeeded=0, failed=" << failed
                      << "). Aborting subsequent tests to protect PIN retries. ***\n"
                      << std::endl;
        }

        return {succeeded, failed};
    }

    static QApplication* app;
    static QTranslator* translator;
    static std::map<QString, QString> pinMap; // cardType -> PIN
    static std::unique_ptr<LibreSCRS::Plugin::CardPluginService> sharedPluginRegistry;
    static std::shared_ptr<LibreSCRS::Trust::TrustStoreService> sharedTrustService;
    static std::shared_ptr<LibreSCRS::Signing::SigningService> sharedSigningService;
    static std::vector<ReaderInfo> sharedCards;
    static std::string suiteSkipReason;
};

QApplication* SigningWizardE2ETest::app = nullptr;
QTranslator* SigningWizardE2ETest::translator = nullptr;
std::map<QString, QString> SigningWizardE2ETest::pinMap;
std::unique_ptr<LibreSCRS::Plugin::CardPluginService> SigningWizardE2ETest::sharedPluginRegistry;
std::shared_ptr<LibreSCRS::Trust::TrustStoreService> SigningWizardE2ETest::sharedTrustService;
std::shared_ptr<LibreSCRS::Signing::SigningService> SigningWizardE2ETest::sharedSigningService;
std::vector<ReaderInfo> SigningWizardE2ETest::sharedCards;
std::string SigningWizardE2ETest::suiteSkipReason;

// ============================================================================
// B-B level tests — card-agnostic, use first available PKI card
// ============================================================================

TEST_F(SigningWizardE2ETest, SignPdf_BB)
{
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString pdfPath = tmpDir.filePath("test.pdf");
    QFile f(pdfPath);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(buildTestPdf());
    f.close();

    auto [succeeded, failed] = runWizardFlow(*card, {pdfPath}, true);
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
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString txtPath = tmpDir.filePath("test.txt");
    QFile f(txtPath);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("Test document for ASiC-E signing.");
    f.close();

    auto [succeeded, failed] = runWizardFlow(*card, {txtPath}, false);
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    // Text files now route to ASiC-E container (.asice)
    QDir outDir(QFileInfo(txtPath).absolutePath());
    QString asicePath = outDir.filePath("test.asice");
    ASSERT_TRUE(QFile::exists(asicePath)) << "ASiC-E container not found: " << asicePath.toStdString();
    QFileInfo fi(asicePath);
    EXPECT_GT(fi.size(), 0);
}

// --- Batch signing tests (2 PDFs + 1 TXT in single wizard run) ---

TEST_F(SigningWizardE2ETest, SignBatch_BB)
{
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    auto pdf = buildTestPdf();
    for (const auto* name : {"doc1.pdf", "doc2.pdf"}) {
        QFile f(tmpDir.filePath(name));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(pdf);
    }
    {
        QFile f(tmpDir.filePath("notes.txt"));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("Batch test document.");
    }

    auto [succeeded, failed] = runWizardFlow(
        *card, {tmpDir.filePath("doc1.pdf"), tmpDir.filePath("doc2.pdf"), tmpDir.filePath("notes.txt")}, true);
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
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString docxPath = tmpDir.filePath("report.docx");
    QFile f(docxPath);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("PK\x03\x04 mock OOXML content");
    f.close();

    auto [succeeded, failed] = runWizardFlow(*card, {docxPath}, false);
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
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString xmlPath = tmpDir.filePath("invoice.xml");
    QFile f(xmlPath);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(buildTestXml());
    f.close();

    auto [succeeded, failed] = runWizardFlow(*card, {xmlPath}, false);
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
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    {
        QFile f(tmpDir.filePath("contract.pdf"));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(buildTestPdf());
    }
    {
        QFile f(tmpDir.filePath("invoice.xml"));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(buildTestXml());
    }
    {
        QFile f(tmpDir.filePath("report.docx"));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("PK\x03\x04 mock OOXML content");
    }

    auto [succeeded, failed] = runWizardFlow(
        *card, {tmpDir.filePath("contract.pdf"), tmpDir.filePath("invoice.xml"), tmpDir.filePath("report.docx")}, true);
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
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString jsonPath = tmpDir.filePath("data.json");
    QFile f(jsonPath);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(R"({"name":"test","value":42})");
    f.close();

    auto [succeeded, failed] = runWizardFlow(*card, {jsonPath}, false);
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
// B-LTA level tests (require trust configuration + TSA)
// Card type does not matter — DSS handles timestamps, revocation data,
// and archive timestamps. The card only signs the hash.
// ============================================================================

TEST_F(SigningWizardE2ETest, SignPdf_BLTA)
{
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString pdfPath = tmpDir.filePath("test.pdf");
    QFile f(pdfPath);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(buildTestPdf());
    f.close();

    auto [succeeded, failed] = runWizardFlow(*card, {pdfPath}, true, 3); // B-LTA
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    QDir outDir(tmpDir.path());
    EXPECT_TRUE(QFile::exists(outDir.filePath("test-signed.pdf")));
}

TEST_F(SigningWizardE2ETest, SignText_BLTA)
{
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    QString txtPath = tmpDir.filePath("test.txt");
    QFile f(txtPath);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("Test ASiC-E B-LTA signing.");
    f.close();

    auto [succeeded, failed] = runWizardFlow(*card, {txtPath}, false, 3); // B-LTA
    EXPECT_EQ(succeeded, 1);
    EXPECT_EQ(failed, 0);

    QDir outDir(tmpDir.path());
    EXPECT_TRUE(QFile::exists(outDir.filePath("test.asice")));
}

TEST_F(SigningWizardE2ETest, SignMixedBatch_BLTA)
{
    const auto* card = firstCard();
    if (!card)
        GTEST_SKIP() << "No PKI-capable card found in any reader";

    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    {
        QFile f(tmpDir.filePath("contract.pdf"));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(buildTestPdf());
    }
    {
        QFile f(tmpDir.filePath("invoice.xml"));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(buildTestXml());
    }
    {
        QFile f(tmpDir.filePath("report.docx"));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("PK\x03\x04 mock OOXML content");
    }

    auto [succeeded, failed] = runWizardFlow(
        *card, {tmpDir.filePath("contract.pdf"), tmpDir.filePath("invoice.xml"), tmpDir.filePath("report.docx")}, true,
        3);
    EXPECT_EQ(succeeded, 3);
    EXPECT_EQ(failed, 0);

    QDir out(tmpDir.path());
    EXPECT_TRUE(QFile::exists(out.filePath("contract-signed.pdf")));
    EXPECT_TRUE(QFile::exists(out.filePath("invoice-signed.xml")));
    EXPECT_TRUE(QFile::exists(out.filePath("report.asice")));
}
