// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "signing/certutils.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <libresign/types.h>

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
// isValidTsaUrl — covers the R4-C1 TSA URL validator used by
// FileSelectionPage::isValid() and SignPage::startSigning() to refuse
// http://, missing-host, and malformed URLs before token exchange.
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
// Format routing tests (exercising the same logic as
// FileSelectionPage::defaultFormatForExtension without accessing private API)
// ---------------------------------------------------------------------------

namespace {

libresign::SignatureFormat defaultFormatForExtension(const QString& suffix)
{
    const QString s = suffix.toLower();
    if (s == QStringLiteral("pdf"))
        return libresign::SignatureFormat::PAdES;
    if (s == QStringLiteral("xml") || s == QStringLiteral("xsd") || s == QStringLiteral("xsl"))
        return libresign::SignatureFormat::XAdES;
    if (s == QStringLiteral("json"))
        return libresign::SignatureFormat::JAdES;
    return libresign::SignatureFormat::ASiC_E;
}

} // namespace

TEST(FormatRouting, PdfGetsPAdES)
{
    EXPECT_EQ(defaultFormatForExtension("pdf"), libresign::SignatureFormat::PAdES);
    EXPECT_EQ(defaultFormatForExtension("PDF"), libresign::SignatureFormat::PAdES);
    EXPECT_EQ(defaultFormatForExtension("Pdf"), libresign::SignatureFormat::PAdES);
}

TEST(FormatRouting, XmlGetsXAdES)
{
    EXPECT_EQ(defaultFormatForExtension("xml"), libresign::SignatureFormat::XAdES);
    EXPECT_EQ(defaultFormatForExtension("xsd"), libresign::SignatureFormat::XAdES);
    EXPECT_EQ(defaultFormatForExtension("xsl"), libresign::SignatureFormat::XAdES);
}

TEST(FormatRouting, JsonGetsJAdES)
{
    EXPECT_EQ(defaultFormatForExtension("json"), libresign::SignatureFormat::JAdES);
}

TEST(FormatRouting, UnknownGetsASiCE)
{
    EXPECT_EQ(defaultFormatForExtension("txt"), libresign::SignatureFormat::ASiC_E);
    EXPECT_EQ(defaultFormatForExtension("docx"), libresign::SignatureFormat::ASiC_E);
    EXPECT_EQ(defaultFormatForExtension("png"), libresign::SignatureFormat::ASiC_E);
    EXPECT_EQ(defaultFormatForExtension(""), libresign::SignatureFormat::ASiC_E);
}

// ---------------------------------------------------------------------------
// buildOutputPath tests (exercising the same logic as SignPage::buildOutputPath)
// ---------------------------------------------------------------------------

namespace {

struct FileSignInfo
{
    QString filePath;
    libresign::SignatureFormat format;
    libresign::SignaturePackaging packaging;
};

QString buildOutputPath(const FileSignInfo& info, const QString& outputDir)
{
    QFileInfo fi(info.filePath);
    QString outputPath;
    switch (info.format) {
    case libresign::SignatureFormat::PAdES:
        outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral("-signed.pdf"));
        break;
    case libresign::SignatureFormat::XAdES:
        if (info.packaging == libresign::SignaturePackaging::ENVELOPED)
            outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral("-signed.xml"));
        else
            outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".xsig"));
        break;
    case libresign::SignatureFormat::ASiC_E:
        outputPath = QDir(outputDir).filePath(fi.completeBaseName() + QStringLiteral(".asice"));
        break;
    case libresign::SignatureFormat::CAdES:
        outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".p7s"));
        break;
    case libresign::SignatureFormat::JAdES:
        outputPath = QDir(outputDir).filePath(fi.fileName() + QStringLiteral(".jose"));
        break;
    }
    return outputPath;
}

} // namespace

TEST(BuildOutputPath, PAdES)
{
    FileSignInfo info{"/tmp/document.pdf", libresign::SignatureFormat::PAdES, libresign::SignaturePackaging::ENVELOPED};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/document-signed.pdf"));
}

TEST(BuildOutputPath, XAdES_Enveloped)
{
    FileSignInfo info{"/tmp/data.xml", libresign::SignatureFormat::XAdES, libresign::SignaturePackaging::ENVELOPED};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/data-signed.xml"));
}

TEST(BuildOutputPath, XAdES_Detached)
{
    FileSignInfo info{"/tmp/data.xml", libresign::SignatureFormat::XAdES, libresign::SignaturePackaging::DETACHED};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/data.xml.xsig"));
}

TEST(BuildOutputPath, ASiCE)
{
    FileSignInfo info{"/tmp/report.docx", libresign::SignatureFormat::ASiC_E, libresign::SignaturePackaging::ENVELOPED};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/report.asice"));
}

TEST(BuildOutputPath, CAdES)
{
    FileSignInfo info{"/tmp/file.bin", libresign::SignatureFormat::CAdES, libresign::SignaturePackaging::DETACHED};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/file.bin.p7s"));
}

TEST(BuildOutputPath, JAdES)
{
    FileSignInfo info{"/tmp/data.json", libresign::SignatureFormat::JAdES, libresign::SignaturePackaging::DETACHED};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/data.json.jose"));
}

TEST(BuildOutputPath, CompoundExtension)
{
    // completeBaseName strips only the last extension
    FileSignInfo info{
        "/tmp/archive.tar.gz", libresign::SignatureFormat::ASiC_E, libresign::SignaturePackaging::ENVELOPED};
    QString result = buildOutputPath(info, "/out");
    EXPECT_EQ(result, QStringLiteral("/out/archive.tar.asice"));
}

// ---------------------------------------------------------------------------
// parseLevel tests (exercising the same logic as SignPage::parseLevel)
// ---------------------------------------------------------------------------

namespace {

libresign::SignatureLevel parseLevel(const QString& level)
{
    if (level == QStringLiteral("B_B"))
        return libresign::SignatureLevel::B_B;
    if (level == QStringLiteral("B_LT"))
        return libresign::SignatureLevel::B_LT;
    if (level == QStringLiteral("B_LTA"))
        return libresign::SignatureLevel::B_LTA;
    return libresign::SignatureLevel::B_T;
}

} // namespace

TEST(ParseLevel, AllLevels)
{
    EXPECT_EQ(parseLevel("B_B"), libresign::SignatureLevel::B_B);
    EXPECT_EQ(parseLevel("B_T"), libresign::SignatureLevel::B_T);
    EXPECT_EQ(parseLevel("B_LT"), libresign::SignatureLevel::B_LT);
    EXPECT_EQ(parseLevel("B_LTA"), libresign::SignatureLevel::B_LTA);
}

TEST(ParseLevel, UnknownDefaultsToBT)
{
    EXPECT_EQ(parseLevel(""), libresign::SignatureLevel::B_T);
    EXPECT_EQ(parseLevel("INVALID"), libresign::SignatureLevel::B_T);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
