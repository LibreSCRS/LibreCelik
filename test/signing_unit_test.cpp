// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

// certutils.h serves the CertUtils DER suites below and nothing else: the TSA
// validator moved out to its own header when the certificate viewer stopped
// parsing DER in-process, so the IsValidTsaUrl suite includes that header
// directly. Both live here until the signing wizard's own re-type retires
// certutils entirely.
#include "certificate/certformat.h"
#include "signing/certutils.h"
#include "signing/tsavalidation.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <LibreSCRS/Signing/Enums.h>

// ---------------------------------------------------------------------------
// certutils tests (public namespace functions, no hardware needed)
// ---------------------------------------------------------------------------

TEST(CertUtils, SubjectCN_EmptyInput)
{
    EXPECT_TRUE(signing::subjectCN({}).isEmpty());
}

TEST(CertUtils, CertNames_EmptyInput)
{
    auto names = signing::certNames({});
    EXPECT_TRUE(names.subjectCN.isEmpty());
    EXPECT_TRUE(names.issuerCN.isEmpty());
}

TEST(CertUtils, CertNames_GarbageInput)
{
    std::vector<uint8_t> garbage = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    auto names = signing::certNames(garbage);
    EXPECT_TRUE(names.subjectCN.isEmpty());
    EXPECT_TRUE(names.issuerCN.isEmpty());
}

TEST(CertUtils, IsCertificateExpired_EmptyInput)
{
    // Empty input is treated as expired (defensive)
    EXPECT_TRUE(signing::isCertificateExpired({}));
}

TEST(CertUtils, IsCertificateExpired_GarbageInput)
{
    std::vector<uint8_t> garbage = {0x01, 0x02, 0x03};
    EXPECT_TRUE(signing::isCertificateExpired(garbage));
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
// Format routing tests (exercising the same logic as
// FileSelectionPage::defaultFormatForExtension without accessing private API)
// ---------------------------------------------------------------------------

namespace {

LibreSCRS::Signing::SignatureFormat defaultFormatForExtension(const QString& suffix)
{
    const QString s = suffix.toLower();
    if (s == QStringLiteral("pdf"))
        return LibreSCRS::Signing::SignatureFormat::Pades;
    if (s == QStringLiteral("xml") || s == QStringLiteral("xsd") || s == QStringLiteral("xsl"))
        return LibreSCRS::Signing::SignatureFormat::Xades;
    if (s == QStringLiteral("json"))
        return LibreSCRS::Signing::SignatureFormat::Jades;
    return LibreSCRS::Signing::SignatureFormat::AsicE;
}

} // namespace

TEST(FormatRouting, PdfGetsPAdES)
{
    EXPECT_EQ(defaultFormatForExtension("pdf"), LibreSCRS::Signing::SignatureFormat::Pades);
    EXPECT_EQ(defaultFormatForExtension("PDF"), LibreSCRS::Signing::SignatureFormat::Pades);
    EXPECT_EQ(defaultFormatForExtension("Pdf"), LibreSCRS::Signing::SignatureFormat::Pades);
}

TEST(FormatRouting, XmlGetsXAdES)
{
    EXPECT_EQ(defaultFormatForExtension("xml"), LibreSCRS::Signing::SignatureFormat::Xades);
    EXPECT_EQ(defaultFormatForExtension("xsd"), LibreSCRS::Signing::SignatureFormat::Xades);
    EXPECT_EQ(defaultFormatForExtension("xsl"), LibreSCRS::Signing::SignatureFormat::Xades);
}

TEST(FormatRouting, JsonGetsJAdES)
{
    EXPECT_EQ(defaultFormatForExtension("json"), LibreSCRS::Signing::SignatureFormat::Jades);
}

TEST(FormatRouting, UnknownGetsASiCE)
{
    EXPECT_EQ(defaultFormatForExtension("txt"), LibreSCRS::Signing::SignatureFormat::AsicE);
    EXPECT_EQ(defaultFormatForExtension("docx"), LibreSCRS::Signing::SignatureFormat::AsicE);
    EXPECT_EQ(defaultFormatForExtension("png"), LibreSCRS::Signing::SignatureFormat::AsicE);
    EXPECT_EQ(defaultFormatForExtension(""), LibreSCRS::Signing::SignatureFormat::AsicE);
}

// ---------------------------------------------------------------------------
// buildOutputPath tests (exercising the same logic as SignPage::buildOutputPath)
// ---------------------------------------------------------------------------

namespace {

// Local mirror of the production FileSignInfo (defined in signing/signpage.h)
// used by these output-path tests. Renamed to avoid a name clash with the
// production type now that signing/signpage.h is included at the bottom of
// this file for the SignPage::needsCanInput predicate tests.
struct BuildPathInput
{
    QString filePath;
    LibreSCRS::Signing::SignatureFormat format;
    LibreSCRS::Signing::PackagingMode packaging;
};

QString buildOutputPath(const BuildPathInput& info, const QString& outputDir)
{
    QFileInfo fi(info.filePath);
    QString outputPath;
    switch (info.format) {
    case LibreSCRS::Signing::SignatureFormat::Pades:
        outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral("-signed.pdf"));
        break;
    case LibreSCRS::Signing::SignatureFormat::Xades:
        if (info.packaging == LibreSCRS::Signing::PackagingMode::Enveloped)
            outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral("-signed.xml"));
        else
            outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".xsig"));
        break;
    case LibreSCRS::Signing::SignatureFormat::AsicE:
        outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral(".asice"));
        break;
    case LibreSCRS::Signing::SignatureFormat::Cades:
        outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".p7s"));
        break;
    case LibreSCRS::Signing::SignatureFormat::Jades:
        outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".jose"));
        break;
    }
    return outputPath;
}

} // namespace

TEST(BuildOutputPath, PAdES)
{
    BuildPathInput info{"/tmp/document.pdf", LibreSCRS::Signing::SignatureFormat::Pades,
                        LibreSCRS::Signing::PackagingMode::Enveloped};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/document-signed.pdf"));
}

TEST(BuildOutputPath, XAdES_Enveloped)
{
    BuildPathInput info{"/tmp/data.xml", LibreSCRS::Signing::SignatureFormat::Xades,
                        LibreSCRS::Signing::PackagingMode::Enveloped};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/data-signed.xml"));
}

TEST(BuildOutputPath, XAdES_Detached)
{
    BuildPathInput info{"/tmp/data.xml", LibreSCRS::Signing::SignatureFormat::Xades,
                        LibreSCRS::Signing::PackagingMode::Detached};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/data.xml.xsig"));
}

TEST(BuildOutputPath, ASiCE)
{
    BuildPathInput info{"/tmp/report.docx", LibreSCRS::Signing::SignatureFormat::AsicE,
                        LibreSCRS::Signing::PackagingMode::Enveloped};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/report.asice"));
}

TEST(BuildOutputPath, CAdES)
{
    BuildPathInput info{"/tmp/file.bin", LibreSCRS::Signing::SignatureFormat::Cades,
                        LibreSCRS::Signing::PackagingMode::Detached};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/file.bin.p7s"));
}

TEST(BuildOutputPath, JAdES)
{
    BuildPathInput info{"/tmp/data.json", LibreSCRS::Signing::SignatureFormat::Jades,
                        LibreSCRS::Signing::PackagingMode::Detached};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/data.json.jose"));
}

TEST(BuildOutputPath, CompoundExtension)
{
    // completeBaseName strips only the last extension
    BuildPathInput info{"/tmp/archive.tar.gz", LibreSCRS::Signing::SignatureFormat::AsicE,
                        LibreSCRS::Signing::PackagingMode::Enveloped};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/archive.tar.asice"));
}

// ---------------------------------------------------------------------------
// parseLevel tests (exercising the same logic as SignPage::parseLevel)
// ---------------------------------------------------------------------------

namespace {

LibreSCRS::Signing::SignatureLevel parseLevel(const QString& level)
{
    if (level == QStringLiteral("B_B"))
        return LibreSCRS::Signing::SignatureLevel::B_B;
    if (level == QStringLiteral("B_LT"))
        return LibreSCRS::Signing::SignatureLevel::B_LT;
    if (level == QStringLiteral("B_LTA"))
        return LibreSCRS::Signing::SignatureLevel::B_LTA;
    return LibreSCRS::Signing::SignatureLevel::B_T;
}

} // namespace

TEST(ParseLevel, AllLevels)
{
    EXPECT_EQ(parseLevel("B_B"), LibreSCRS::Signing::SignatureLevel::B_B);
    EXPECT_EQ(parseLevel("B_T"), LibreSCRS::Signing::SignatureLevel::B_T);
    EXPECT_EQ(parseLevel("B_LT"), LibreSCRS::Signing::SignatureLevel::B_LT);
    EXPECT_EQ(parseLevel("B_LTA"), LibreSCRS::Signing::SignatureLevel::B_LTA);
}

TEST(ParseLevel, UnknownDefaultsToBT)
{
    EXPECT_EQ(parseLevel(""), LibreSCRS::Signing::SignatureLevel::B_T);
    EXPECT_EQ(parseLevel("INVALID"), LibreSCRS::Signing::SignatureLevel::B_T);
}

// ---------------------------------------------------------------------------
// SignPage::needsCanInput — pure predicate gating CAN row visibility.
// True iff the reader is contactless AND no SM tunnel is yet up on the
// shared CardSession.
// ---------------------------------------------------------------------------

#include "signing/signpage.h"

TEST(NeedsCanInput, ContactReaderHidesRow)
{
    EXPECT_FALSE(SignPage::needsCanInput(/*isCL=*/false, /*smAlreadyUp=*/false));
    EXPECT_FALSE(SignPage::needsCanInput(/*isCL=*/false, /*smAlreadyUp=*/true));
}

TEST(NeedsCanInput, ContactlessWithLiveSmHidesRow)
{
    // After LC 8ed0c75: display flow established PACE+CAN on the shared
    // session; wizard does not need to re-collect CAN.
    EXPECT_FALSE(SignPage::needsCanInput(/*isCL=*/true, /*smAlreadyUp=*/true));
}

TEST(NeedsCanInput, ContactlessWithoutLiveSmShowsRow)
{
    // Fallback: wizard opened before display flow deposited CAN, or
    // hasLiveSecureChannel() returned false on lock failure. User must
    // re-enter CAN; PACE re-runs idempotently.
    EXPECT_TRUE(SignPage::needsCanInput(/*isCL=*/true, /*smAlreadyUp=*/false));
}

TEST(NeedsCanInput, NoexceptContract)
{
    // The predicate must be noexcept so SignPage::configure can call it
    // without exception-safety bookkeeping.
    static_assert(noexcept(SignPage::needsCanInput(true, false)));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
