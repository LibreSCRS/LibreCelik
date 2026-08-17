// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

// Nothing here parses DER any more: expiry is a typed field on the record the
// agent hands over, and the TSA validator has its own header since the
// certificate viewer stopped parsing in-process.
#include "certificate/certformat.h"
#include "signing/filedropzone.h"
#include "signing/fileselectionpage.h"
#include "signing/tsavalidation.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <LibreSCRS/AgentClient/SignOptions.h>
#include <LibreSCRS/AgentClient/Types.h>

// ---------------------------------------------------------------------------
// Expired-certificate consent — the predicate the sign page asks before it
// signs at the baseline level, now a comparison against the record's own
// validity window instead of a DER parse.
// ---------------------------------------------------------------------------

TEST(ExpiredCertConsent, ValidityWindowDecidesAndAnAbsentEndIsTreatedAsPast)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();

    LibreSCRS::AgentClient::CertificateInfo expired;
    expired.notAfter = now.addDays(-1);
    EXPECT_TRUE(expired.notAfter < now); // consent is asked

    LibreSCRS::AgentClient::CertificateInfo live;
    live.notAfter = now.addDays(1);
    EXPECT_FALSE(live.notAfter < now); // nothing to consent to

    // An agent that reported no validity end at all leaves the member default
    // constructed, and an invalid QDateTime orders before every valid one — so
    // the unknown case still lands on the consent prompt. That is the same
    // defensive posture the retired DER parse took for input it could not
    // read, kept by construction rather than by a special case.
    const LibreSCRS::AgentClient::CertificateInfo unknown;
    ASSERT_FALSE(unknown.notAfter.isValid());
    EXPECT_TRUE(unknown.notAfter < now);
}

// ---------------------------------------------------------------------------
// isValidTsaUrl — TSA URL validator used by FileSelectionPage::isValid()
// and SignPage::startSigning() to refuse http://, missing-host, and
// malformed URLs before token exchange.
// ---------------------------------------------------------------------------

TEST(IsValidTsaUrl, EmptyRejected)
{
    EXPECT_FALSE(signing::isValidTsaUrl(QStringLiteral("")));
}

// ---------------------------------------------------------------------------
// tsaUrlForLevel — the ONE decision of whether a sign request carries the
// configured TSA URL at all. The agent refuses `tsaUrl` outside the
// timestamped/long-term family (b-t/b-lt/b-lta) — a B_B request that
// forwards a configured URL anyway is refused at submit, before any card
// work (the Leg-1 bench catch, 2026-08-17).
// ---------------------------------------------------------------------------

TEST(TsaUrlForLevel, BaselineLevelNeverCarriesTheConfiguredUrl)
{
    EXPECT_TRUE(
        signing::tsaUrlForLevel(QStringLiteral("B_B"), QStringLiteral("https://tsa.example.com/rfc3161")).isEmpty());
}

TEST(TsaUrlForLevel, TimestampedFamilyCarriesItVerbatim)
{
    const QString url = QStringLiteral("https://tsa.example.com/rfc3161");
    EXPECT_EQ(signing::tsaUrlForLevel(QStringLiteral("B_T"), url), url);
    EXPECT_EQ(signing::tsaUrlForLevel(QStringLiteral("B_LT"), url), url);
    EXPECT_EQ(signing::tsaUrlForLevel(QStringLiteral("B_LTA"), url), url);
}

TEST(TsaUrlForLevel, EmptyUrlStaysEmptyForEveryLevel)
{
    EXPECT_TRUE(signing::tsaUrlForLevel(QStringLiteral("B_B"), QString()).isEmpty());
    EXPECT_TRUE(signing::tsaUrlForLevel(QStringLiteral("B_LTA"), QString()).isEmpty());
}

TEST(IsValidTsaUrl, HttpRejected)
{
    EXPECT_FALSE(signing::isValidTsaUrl(QStringLiteral("http://example.com/tsa")));
}

TEST(IsValidTsaUrl, MissingHostRejected)
{
    EXPECT_FALSE(signing::isValidTsaUrl(QStringLiteral("https://")));
    EXPECT_FALSE(signing::isValidTsaUrl(QStringLiteral("https:///path")));
}

TEST(IsValidTsaUrl, MalformedRejected)
{
    // StrictMode catches a missing slash in the authority marker
    EXPECT_FALSE(signing::isValidTsaUrl(QStringLiteral("https:/example.com")));
    EXPECT_FALSE(signing::isValidTsaUrl(QStringLiteral("not a url")));
}

TEST(IsValidTsaUrl, FileSchemeRejected)
{
    EXPECT_FALSE(signing::isValidTsaUrl(QStringLiteral("file:///etc/passwd")));
    EXPECT_FALSE(signing::isValidTsaUrl(QStringLiteral("ftp://example.com/")));
}

TEST(IsValidTsaUrl, HttpsAccepted)
{
    EXPECT_TRUE(signing::isValidTsaUrl(QStringLiteral("https://tsa.example.com/timestamp")));
    EXPECT_TRUE(signing::isValidTsaUrl(QStringLiteral("https://tsa.example.com")));
    EXPECT_TRUE(signing::isValidTsaUrl(QStringLiteral("https://tsa.example.com:8443/ts")));
}

// ---------------------------------------------------------------------------
// certformat KeyUsage rendering — the agent hands the certificate's KeyUsage
// as the RFC 5280 §4.2.1.3 bitmask (bit i == ordinal i), so the formatter now
// takes that mask where it used to take a parsed-certificate enum span. The
// rendered text must be exactly what it has always been: the same localized
// per-bit labels, in ascending ordinal order, comma-separated.
// ---------------------------------------------------------------------------

namespace cf = librecelik::certformat;

TEST(CertFormat, KeyUsageToStringRendersEveryBitInOrdinalOrder)
{
    // digitalSignature(0) | keyCertSign(5) | decipherOnly(8)
    const quint32 bits = (1u << 0) | (1u << 5) | (1u << 8);
    const QString expected = QStringList{qtTrId("lc-token-ku-digital-signature"), qtTrId("lc-cert-ku-key-cert-sign"),
                                         qtTrId("lc-cert-ku-decipher-only")}
                                 .join(QStringLiteral(", "));
    EXPECT_EQ(cf::keyUsageToString(bits), expected);
}

TEST(CertFormat, KeyUsageToStringRendersAllNineBits)
{
    const QString expected =
        QStringList{qtTrId("lc-token-ku-digital-signature"), qtTrId("lc-token-ku-non-repudiation"),
                    qtTrId("lc-token-ku-key-encipherment"),  qtTrId("lc-token-ku-data-encipherment"),
                    qtTrId("lc-token-ku-key-agreement"),     qtTrId("lc-cert-ku-key-cert-sign"),
                    qtTrId("lc-cert-ku-crl-sign"),           qtTrId("lc-cert-ku-encipher-only"),
                    qtTrId("lc-cert-ku-decipher-only")}
            .join(QStringLiteral(", "));
    EXPECT_EQ(cf::keyUsageToString(0x1FFu), expected);
}

TEST(CertFormat, KeyUsageToStringIsEmptyForNoBits)
{
    EXPECT_TRUE(cf::keyUsageToString(0u).isEmpty());
}

TEST(CertFormat, KeyUsageToStringIgnoresOrdinalsBeyondRfc5280)
{
    // A newer producer setting a bit this build has no label for must not
    // inject an empty comma-separated fragment into the rendered list.
    EXPECT_EQ(cf::keyUsageToString((1u << 0) | (1u << 9) | (1u << 31)), qtTrId("lc-token-ku-digital-signature"));
}

TEST(CertFormat, KeyUsageToStringEndEntitySuppressesCaAndCipherOnlyBits)
{
    // The token summary shows end-entity capability only: keyCertSign(5),
    // cRLSign(6), encipherOnly(7) and decipherOnly(8) are deliberately not
    // rendered there, exactly as before the re-type.
    const QString expected = QStringList{qtTrId("lc-token-ku-digital-signature"), qtTrId("lc-token-ku-non-repudiation"),
                                         qtTrId("lc-token-ku-key-encipherment"),
                                         qtTrId("lc-token-ku-data-encipherment"), qtTrId("lc-token-ku-key-agreement")}
                                 .join(QStringLiteral(", "));
    EXPECT_EQ(cf::keyUsageToStringEndEntity(0x1FFu), expected);
}

TEST(CertFormat, KeyUsageBitLabelIsEmptyForAnUnknownOrdinal)
{
    EXPECT_TRUE(cf::keyUsageBitLabel(9).isEmpty());
    EXPECT_TRUE(cf::keyUsageBitLabel(-1).isEmpty());
    EXPECT_EQ(cf::keyUsageBitLabel(0), qtTrId("lc-token-ku-digital-signature"));
}

// ---------------------------------------------------------------------------
// Format routing tests — these call the REAL
// FileSelectionPage::defaultFormatForExtension (public precisely so this suite
// can pin it) on the client's own signature-format vocabulary. The suite used
// to carry a test-local copy of the routing table, which pinned the copy
// rather than the page.
// ---------------------------------------------------------------------------

namespace ac = LibreSCRS::AgentClient;

TEST(FormatRouting, PdfGetsPAdES)
{
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("pdf"), ac::SignatureFormat::PAdES);
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("PDF"), ac::SignatureFormat::PAdES);
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("Pdf"), ac::SignatureFormat::PAdES);
}

TEST(FormatRouting, XmlGetsXAdES)
{
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("xml"), ac::SignatureFormat::XAdES);
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("xsd"), ac::SignatureFormat::XAdES);
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("xsl"), ac::SignatureFormat::XAdES);
}

TEST(FormatRouting, JsonGetsJAdES)
{
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("json"), ac::SignatureFormat::JAdES);
}

TEST(FormatRouting, UnknownGetsASiCE)
{
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("txt"), ac::SignatureFormat::ASiCe);
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("docx"), ac::SignatureFormat::ASiCe);
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension("png"), ac::SignatureFormat::ASiCe);
    EXPECT_EQ(FileSelectionPage::defaultFormatForExtension(""), ac::SignatureFormat::ASiCe);
}

// ---------------------------------------------------------------------------
// FileSelectionPage widget cases — the selection cap and the capability
// gating, driven through the real page.
//
// A QWidget built without a QApplication aborts the process, so these ride a
// fixture that guarantees one is installed; the pure routing cases above need
// no such thing and stay bare TEST()s.
// ---------------------------------------------------------------------------

class FileSelectionPageTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            static int argc = 0;
            app = new QApplication(argc, nullptr);
        }
        // The page reads its defaults (level, TSA list, output folder) from
        // QSettings; keep that off the developer's real configuration.
        QStandardPaths::setTestModeEnabled(true);
    }

    // FileDropZone::addFiles() filters everything that is not an existing,
    // readable regular file, so a scripted selection has to exist on disk.
    QStringList makeFiles(int count)
    {
        QStringList paths;
        for (int i = 0; i < count; ++i) {
            const QString path = dir.filePath(QStringLiteral("doc%1.pdf").arg(i));
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly))
                return {};
            file.write("%PDF-1.4\n");
            file.close();
            paths.append(path);
        }
        return paths;
    }

    static FileDropZone* dropZoneOf(FileSelectionPage& page)
    {
        return page.findChild<FileDropZone*>();
    }

    static QString labelText(FileSelectionPage& page, const QString& objectName)
    {
        auto* label = page.findChild<QLabel*>(objectName);
        return label ? label->text() : QString();
    }

    QTemporaryDir dir;
    static QApplication* app;
};
QApplication* FileSelectionPageTest::app = nullptr;

TEST_F(FileSelectionPageTest, SelectionBeyondTheBatchBoundIsRefusedAndSaysSo)
{
    ASSERT_TRUE(dir.isValid());
    const int overBound = static_cast<int>(ac::kMaxBatchDocuments) + 1;
    const QStringList paths = makeFiles(overBound);
    ASSERT_EQ(paths.count(), overBound);

    FileSelectionPage page;
    auto* zone = dropZoneOf(page);
    ASSERT_NE(zone, nullptr);

    zone->addFiles(paths);

    EXPECT_EQ(page.selectedFiles(), paths.mid(0, static_cast<int>(ac::kMaxBatchDocuments)));
    EXPECT_EQ(labelText(page, QStringLiteral("limitsLabel")),
              qtTrId("lc-sign-too-many-files").arg(ac::kMaxBatchDocuments));
}

TEST_F(FileSelectionPageTest, SelectionAtTheBatchBoundIsAcceptedWholeAndKeepsTheLimitsText)
{
    ASSERT_TRUE(dir.isValid());
    const QStringList paths = makeFiles(static_cast<int>(ac::kMaxBatchDocuments));
    ASSERT_EQ(paths.count(), static_cast<int>(ac::kMaxBatchDocuments));

    FileSelectionPage page;
    auto* zone = dropZoneOf(page);
    ASSERT_NE(zone, nullptr);

    zone->addFiles(paths);

    EXPECT_EQ(page.selectedFiles(), paths);
    EXPECT_EQ(labelText(page, QStringLiteral("limitsLabel")), qtTrId("lc-sign-limits-info"));
}

TEST_F(FileSelectionPageTest, AnAgentWithoutTsaOverrideHidesTheTsaRow)
{
    FileSelectionPage page;
    page.setCapabilities(/*tsaOverride=*/false, /*batch=*/true);

    auto* tsaRow = page.findChild<QWidget*>(QStringLiteral("tsaRow"));
    ASSERT_NE(tsaRow, nullptr);
    EXPECT_FALSE(tsaRow->isVisibleTo(&page));
}

TEST_F(FileSelectionPageTest, ABatchLessAgentAnnouncesOneConfirmationPerFile)
{
    ASSERT_TRUE(dir.isValid());
    const QStringList paths = makeFiles(2);
    ASSERT_EQ(paths.count(), 2);

    FileSelectionPage page;
    auto* zone = dropZoneOf(page);
    ASSERT_NE(zone, nullptr);
    zone->addFiles(paths);

    auto* notice = page.findChild<QLabel*>(QStringLiteral("sequentialNotice"));
    ASSERT_NE(notice, nullptr);

    // A batch-capable agent signs the run in one ceremony: no notice.
    page.setCapabilities(/*tsaOverride=*/true, /*batch=*/true);
    EXPECT_FALSE(notice->isVisibleTo(&page));

    // A pre-feature agent degrades to one confirmation per file, and says so.
    page.setCapabilities(/*tsaOverride=*/true, /*batch=*/false);
    EXPECT_TRUE(notice->isVisibleTo(&page));
    EXPECT_EQ(notice->text(), qtTrId("lc-sign-sequential-notice", 2));
}

int main(int argc, char** argv)
{
    // QApplication, not QCoreApplication: the target now compiles the real
    // FileSelectionPage, and a QWidget built with only a core application
    // instance aborts the process (the fixture's guard cannot install one
    // afterwards — QCoreApplication::instance() is already non-null).
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
