// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "document/tokensection.h"

#include "certificate/certificatepropertiesmodel.h"

#include <LibreSCRS/AgentClient/CredentialTypes.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <gtest/gtest.h>

using LibreSCRS::AgentClient::CertificateInfo;
using LibreSCRS::AgentClient::CredentialKind;
using LibreSCRS::AgentClient::CredentialRecord;
using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;
using LibreSCRS::AgentClient::TrustStatus;

namespace {

class TokenSectionPlaceholderTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app = new QApplication(argc, argv);
        }

        // Counter rendering substitutes numbers into a translated template
        // ("Attempts: %1 of %2"). With no translator installed qtTrId hands
        // back the bare catalog ID, which carries no place markers, so the
        // numbers the counter assertions look for would be dropped by arg().
        // Install the English catalog the way SlotLabelFormatterTests and
        // i18n_settings_state_test do — the .qm directory is baked in at
        // compile time by test/CMakeLists.txt.
        if (!translator) {
            translator = new QTranslator();
            const QString qmDir = QStringLiteral(LIBRECELIK_TRANSLATIONS_DIR_DEFAULT);
            ASSERT_TRUE(translator->load(QStringLiteral("LibreCelik_en"), qmDir))
                << "failed to load LibreCelik_en.qm from " << qmDir.toStdString();
            QCoreApplication::installTranslator(translator);
        }
    }
    static QApplication* app;
    static QTranslator* translator;
};
QApplication* TokenSectionPlaceholderTest::app = nullptr;
QTranslator* TokenSectionPlaceholderTest::translator = nullptr;

// The two …ForTest helpers walk the rendered tree the way the placeholder
// assertions below walk the rendered header card: the tree is the section's
// only QTreeWidget child, and the ctor builds the certificate root first and
// the credential root second, so their top-level indices are fixed.
[[nodiscard]] QString certDetailTextForTest(const TokenSection& section, int certIndex)
{
    auto* tree = section.findChild<QTreeWidget*>();
    if (!tree)
        return {};
    QTreeWidgetItem* certsRoot = tree->topLevelItem(0);
    if (!certsRoot || certIndex < 0 || certIndex >= certsRoot->childCount())
        return {};

    QTreeWidgetItem* certItem = certsRoot->child(certIndex);
    QStringList columns{certItem->text(1)};
    for (int row = 0; row < certItem->childCount(); ++row)
        columns << certItem->child(row)->text(1);
    return columns.join(QLatin1Char(' '));
}

[[nodiscard]] QString credentialRowTextForTest(const TokenSection& section, int credentialIndex)
{
    auto* tree = section.findChild<QTreeWidget*>();
    if (!tree)
        return {};
    QTreeWidgetItem* credentialRoot = tree->topLevelItem(1);
    if (!credentialRoot || credentialIndex < 0 || credentialIndex >= credentialRoot->childCount())
        return {};

    QTreeWidgetItem* item = credentialRoot->child(credentialIndex);
    return item->text(0) + QLatin1Char(' ') + item->text(1);
}

// Regression guard: when a card read produces no token fields (empty
// FieldGroup), the header card must still render so users see the
// structural UX even with no card data available. Pre-fix, an early
// `if (group.fields.empty()) return;` short-circuited rendering entirely.
TEST_F(TokenSectionPlaceholderTest, EmptyGroupStillRendersHeader)
{
    TokenSection section(nullptr);
    FieldGroup empty;

    section.setTokenInfo(empty);

    // Header card has 3 field-label QLabels (one per row) + 1 icon QLabel = 4.
    // Empty-group fallback path must produce a CardHeaderCard child.
    EXPECT_GT(section.findChildren<QLabel*>().size(), 0)
        << "Header card must render even when middleware returns empty group";
}

// Regression guard: when a known field is present but its value is empty,
// the rendered value cell must show the em-dash placeholder U+2014 rather
// than an empty / missing row.
TEST_F(TokenSectionPlaceholderTest, EmptyValueRendersEmDashPlaceholder)
{
    TokenSection section(nullptr);
    FieldGroup group;
    group.key = QStringLiteral("pki");

    Field labelField;
    labelField.key = QStringLiteral("label");
    // value intentionally empty — exercises placeholder render
    group.fields.append(labelField);

    section.setTokenInfo(group);

    const auto values = section.findChildren<QLineEdit*>();
    const QString emDash = QString::fromUtf8("\xe2\x80\x94");
    bool hasPlaceholder = false;
    for (auto* v : values) {
        if (v->text() == emDash) {
            hasPlaceholder = true;
            break;
        }
    }
    EXPECT_TRUE(hasPlaceholder) << "Empty value must render em-dash (U+2014) placeholder";
}

TEST_F(TokenSectionPlaceholderTest, SecurityStatusTokensRenderLocalizedAndUnknownVerbatim)
{
    using namespace LibreSCRS::AgentClient;
    TokenSection ts;
    CertificateInfo c;
    c.id = QStringLiteral("cert-1");
    c.subject = QStringLiteral("CN=Test");
    c.trust = TrustStatus::Untrusted;
    c.securityStatus = {QStringLiteral("untrusted-root"), QStringLiteral("future-token")};
    ts.setCertificates({c});
    const QString detail = certDetailTextForTest(ts, 0); // walks the tree item's status column
    EXPECT_TRUE(detail.contains(qtTrId("lc-cert-status-untrusted-root")));
    EXPECT_TRUE(detail.contains(QStringLiteral("future-token"))); // unknown token verbatim
}

// The certificate rows show what key a certificate carries. The data is the
// agent's public-key group, and the rendering is the one this section has
// always used: "<algorithm> <bits>-bit (<curve>)".
TEST_F(TokenSectionPlaceholderTest, CertificateRowRendersTheAlgorithmLineFromThePublicKeyGroup)
{
    using namespace LibreSCRS::AgentClient;
    TokenSection ts;
    CertificateInfo c;
    c.id = QStringLiteral("cert-1");
    c.subject = QStringLiteral("CN=Test");
    c.extra.insert(
        QStringLiteral("fields"),
        QVariantMap{
            {QStringLiteral("publicKey"),
             QVariantMap{
                 {QStringLiteral("algorithm"), QVariantList{QStringLiteral("cert.publicKey.algorithm"),
                                                            QStringLiteral("Algorithm"), QStringLiteral("ECDSA")}},
                 {QStringLiteral("sizeBits"), QVariantList{QStringLiteral("cert.publicKey.sizeBits"),
                                                           QStringLiteral("Key Size"), QStringLiteral("256")}},
                 {QStringLiteral("curveOid"),
                  QVariantList{QStringLiteral("cert.publicKey.curveOid"), QStringLiteral("Curve"),
                               QStringLiteral("1.2.840.10045.3.1.7")}}}}});
    ts.setCertificates({c});
    const QString detail = certDetailTextForTest(ts, 0);
    EXPECT_TRUE(detail.contains(QStringLiteral("ECDSA 256-bit (1.2.840.10045.3.1.7)"))) << detail.toStdString();
}

// The key-usage line summarises end-entity capability only — the CA and
// cipher-direction bits belong to the certificate viewer, not to this summary.
TEST_F(TokenSectionPlaceholderTest, CertificateRowRendersTheEndEntityKeyUsageLine)
{
    using namespace LibreSCRS::AgentClient;
    TokenSection ts;
    CertificateInfo c;
    c.id = QStringLiteral("cert-1");
    c.subject = QStringLiteral("CN=Test");
    // digitalSignature(0) | nonRepudiation(1) | keyCertSign(5): the last one
    // is a CA bit and must not appear in this summary.
    c.keyUsageBits = (1u << 0) | (1u << 1) | (1u << 5);
    ts.setCertificates({c});
    const QString detail = certDetailTextForTest(ts, 0);
    EXPECT_TRUE(detail.contains(qtTrId("lc-token-ku-digital-signature"))) << detail.toStdString();
    EXPECT_TRUE(detail.contains(qtTrId("lc-token-ku-non-repudiation"))) << detail.toStdString();
    EXPECT_FALSE(detail.contains(qtTrId("lc-cert-ku-key-cert-sign"))) << detail.toStdString();
}

TEST_F(TokenSectionPlaceholderTest, CredentialCountersRenderRetriesAndUses)
{
    using namespace LibreSCRS::AgentClient;
    TokenSection ts;
    CredentialRecord r;
    r.id = QStringLiteral("pin-1");
    r.label = QStringLiteral("User PIN");
    r.kind = CredentialKind::User;
    r.retriesLeft = 2;
    r.retriesMax = 3;
    ts.setCredentials({r});
    const QString row = credentialRowTextForTest(ts, 0);
    EXPECT_TRUE(row.contains(QStringLiteral("2")));
    EXPECT_TRUE(row.contains(QStringLiteral("3")));
}

// --- certificate viewer: criticality arrives as a cell, renders as a mark ---
//
// The properties model lives in this target because the target already
// compiles the whole certificate display layer; these two helpers walk its
// Extensions subtree the way the tree view paints it.

[[nodiscard]] QModelIndex extensionsIndexForTest(const QAbstractItemModel& model)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex idx = model.index(row, 0);
        if (idx.data(Qt::DisplayRole).toString() == qtTrId("lc-cert-field-extensions"))
            return idx;
    }
    return {};
}

/// Separator for the flattened row form below. A unit separator cannot occur
/// in a rendered label or value, so a joined row is unambiguous — and it is
/// built as a QChar, never as a `\x1f` escape inside a literal, because an
/// adjacent hex digit would silently widen that escape into a different
/// character.
const QChar kRowSeparator(QChar(0x1f));

/// "<label>|<value>|<bold?1:0>" (separator above) for every extension row, in
/// the order the tree paints them.
[[nodiscard]] QStringList extensionRowsForTest(const QAbstractItemModel& model)
{
    QStringList rows;
    const QModelIndex extensions = extensionsIndexForTest(model);
    if (!extensions.isValid())
        return rows;
    for (int row = 0; row < model.rowCount(extensions); ++row) {
        const QModelIndex labelIdx = model.index(row, 0, extensions);
        const QModelIndex valueIdx = model.index(row, 1, extensions);
        rows << labelIdx.data(Qt::DisplayRole).toString() + kRowSeparator + valueIdx.data(Qt::DisplayRole).toString() +
                    kRowSeparator + (labelIdx.data(Qt::FontRole).isValid() ? QStringLiteral("1") : QStringLiteral("0"));
    }
    return rows;
}

/// The flattened form of one expected row.
[[nodiscard]] QString expectedRowForTest(const QString& label, const QString& value, bool bold)
{
    return label + kRowSeparator + value + kRowSeparator + (bold ? QStringLiteral("1") : QStringLiteral("0"));
}

TEST_F(TokenSectionPlaceholderTest, CriticalCellMarksTheGroupRowAndIsNeverARowItself)
{
    using namespace LibreSCRS::AgentClient;

    const QVariantList criticalCell{QString(), QStringLiteral("Critical"), QStringLiteral("true")};

    CertificateInfo c;
    c.id = QStringLiteral("cert-1");
    c.subject = QStringLiteral("CN=Test");
    c.extra.insert(
        QStringLiteral("fields"),
        QVariantMap{
            {QStringLiteral("basicConstraints"),
             QVariantMap{{QStringLiteral("isCa"), QVariantList{QStringLiteral("cert.basicConstraints.isCa"),
                                                               QStringLiteral("CA"), QStringLiteral("false")}},
                         {QStringLiteral("critical"), criticalCell}}},
            {QStringLiteral("san"),
             QVariantMap{{QStringLiteral("dns0"), QVariantList{QStringLiteral("cert.san.dns"), QStringLiteral("DNS"),
                                                               QStringLiteral("example.invalid")}},
                         {QStringLiteral("critical"), criticalCell}}},
            {QStringLiteral("crlDp"),
             QVariantMap{{QStringLiteral("url0"),
                          QVariantList{QStringLiteral("cert.crlDp.url0"), QStringLiteral("CRL Distribution Point"),
                                       QStringLiteral("http://crl.invalid/a")}}}}});

    CertificatePropertiesModel model(c);
    const QStringList rows = extensionRowsForTest(model);
    const QString all = rows.join(QLatin1Char('\n'));

    const QString suffix = qtTrId("lc-cert-extension-critical");
    // The two critical groups are marked — localized suffix on the label AND
    // the bold font the tree paints critical extensions with.
    EXPECT_TRUE(rows.contains(
        expectedRowForTest(QStringLiteral("Basic Constraints") + suffix, QStringLiteral("CA: FALSE"), true)))
        << all.toStdString();
    EXPECT_TRUE(rows.contains(expectedRowForTest(QStringLiteral("Subject Alternative Name") + suffix,
                                                 QStringLiteral("DNS: example.invalid"), true)))
        << all.toStdString();
    // The group without the cell stays unmarked and unbolded.
    EXPECT_TRUE(rows.contains(
        expectedRowForTest(QStringLiteral("CRL Distribution Points"), QStringLiteral("http://crl.invalid/a"), false)))
        << all.toStdString();
    // And the cell itself is metadata: it never becomes a row of its own, and
    // never leaks its value into the group it marks.
    EXPECT_FALSE(all.contains(QStringLiteral("Critical") + kRowSeparator)) << all.toStdString();
    EXPECT_FALSE(all.contains(QStringLiteral("true"))) << all.toStdString();
}

} // namespace
