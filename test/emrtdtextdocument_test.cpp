// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief What the PRINTED travel-document record says about the checks that
///        did not run.
///
/// The window and the paper are two renderings of one read, and only one of
/// them leaves the building. A printout is what somebody attaches as evidence
/// that a passport was checked, so it has to carry the same admission the
/// screen makes: which checks did not run, and why. A bare
/// "Data Authenticity: Not Performed" with nothing beside it overstates the
/// document by omission — the reader cannot tell an unconfigured trust store
/// from a passport nobody could vouch for.
///
/// Two properties these cases exist to hold:
///
///  1. Every assertion is on RENDERED output — the QTextDocument's own HTML or
///     plain text — never on a model the renderer might ignore.
///  2. The reason is a KEY on the wire ("csca.not-configured") and a sentence
///     on paper. Asserting only that the sentence appears passes on a build
///     with no catalogue entry, because qtTrId() answers with the bare id; so
///     each case also names the id as something that must NOT reach the page.

#include "emrtdtextdocument.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTranslator>

#include <gtest/gtest.h>

#include <utility>

using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

namespace {

/// Exposes the rendered document: these cases assert on what the printer is
/// handed, not on anything the formatter kept to itself.
class TestableEMRTDTextDocument : public EMRTDTextDocument
{
public:
    using EMRTDTextDocument::EMRTDTextDocument;

    QString html() const
    {
        return document.toHtml();
    }

    QString text() const
    {
        return document.toPlainText();
    }
};

Field textField(const QString& key, const QString& value)
{
    Field field;
    field.key = key;
    field.value = value;
    field.extra.insert(QStringLiteral("labelFallback"), key);
    field.extra.insert(QStringLiteral("type"), QStringLiteral("text"));
    return field;
}

FieldGroup group(const QString& key, QList<Field> fields)
{
    FieldGroup g;
    g.key = key;
    g.fields = std::move(fields);
    return g;
}

/// One check in the wire's `check_<N>_<suffix>` shape.
QList<Field> check(int index, const QString& id, const QString& category, const QString& status, const QString& label,
                   const QString& reason = {})
{
    const auto name = [index](const char* suffix) {
        return QStringLiteral("check_%1_%2").arg(index).arg(QLatin1StringView(suffix));
    };
    QList<Field> fields{
        textField(name("id"), id),
        textField(name("category"), category),
        textField(name("status"), status),
        textField(name("label"), label),
    };
    if (!reason.isEmpty()) {
        fields.append(textField(name("reason"), reason));
    }
    return fields;
}

/// A minimal read: the personal group a printout always carries, plus a
/// security group built from @p securityFields.
QList<FieldGroup> readWith(QList<Field> securityFields)
{
    QList<FieldGroup> groups;
    groups.append(
        group(QStringLiteral("personal"), {textField(QStringLiteral("surname"), QStringLiteral("PETROVIC")),
                                           textField(QStringLiteral("given_names"), QStringLiteral("MARKO"))}));
    groups.append(group(QStringLiteral("security_status"), std::move(securityFields)));
    return groups;
}

class EmrtdTextDocumentTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QApplication::instance()) {
            // Static: QApplication keeps a reference to argc for its lifetime.
            static int argc = 0;
            app = new QApplication(argc, nullptr);
        }
        if (translator != nullptr) {
            return;
        }
        // Without a catalogue qtTrId() hands back the bare id, and every case
        // below that compares a sentence would be comparing an id with itself.
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
QApplication* EmrtdTextDocumentTest::app = nullptr;
QTranslator* EmrtdTextDocumentTest::translator = nullptr;

} // namespace

// --- the gap this work closes -----------------------------------------------

// The printed record carried the three aggregate verdicts and nothing else, so
// "Data Authenticity: Not Performed" reached the page with no way to tell an
// empty trust store from a document that failed. The screen had already
// learned to say which.
TEST_F(EmrtdTextDocumentTest, CheckThatDidNotRunPrintsItsReason)
{
    TestableEMRTDTextDocument doc(readWith(
        QList<Field>{textField(QStringLiteral("overall_authenticity"), QStringLiteral("NOT_PERFORMED"))} +
        check(0, QStringLiteral("passive_auth"), QStringLiteral("data_authenticity"), QStringLiteral("NOT_PERFORMED"),
              QStringLiteral("Passive Authentication"), QStringLiteral("csca.not-configured"))));

    const QString printed = doc.text();
    EXPECT_TRUE(printed.contains(QStringLiteral("Passive Authentication")))
        << "the check itself never reached the page";
    EXPECT_TRUE(printed.contains(qtTrId("lc-emrtd-security-details"))) << "the per-check block has no heading on paper";
    EXPECT_TRUE(printed.contains(qtTrId("lc-emrtd-csca-not-configured")))
        << "the reason never reached the page; printed: " << qPrintable(printed);
    EXPECT_FALSE(printed.contains(QStringLiteral("csca.not-configured"))) << "the wire key reached the page raw";
    // The half that fails when the catalogue entry is deleted: qtTrId() then
    // returns the id, and the EXPECT_TRUE above compares it with itself.
    EXPECT_FALSE(printed.contains(QStringLiteral("lc-emrtd-csca-not-configured")))
        << "no catalogue entry; the id itself reached the page";
}

// --- and stays readable -----------------------------------------------------

// A wall of rows is its own failure. The paper explains what did NOT happen;
// a check that ran gets its one terse line and no paragraph under it.
TEST_F(EmrtdTextDocumentTest, CheckThatPassedStaysTerse)
{
    TestableEMRTDTextDocument doc(readWith(
        QList<Field>{textField(QStringLiteral("overall_authenticity"), QStringLiteral("NOT_PERFORMED"))} +
        check(0, QStringLiteral("chip_auth"), QStringLiteral("chip_genuineness"), QStringLiteral("PASSED"),
              QStringLiteral("Chip Authentication"), QStringLiteral("csca.chain-failed")) +
        check(1, QStringLiteral("passive_auth"), QStringLiteral("data_authenticity"), QStringLiteral("NOT_PERFORMED"),
              QStringLiteral("Passive Authentication"), QStringLiteral("csca.not-configured"))));

    const QString printed = doc.text();
    EXPECT_TRUE(printed.contains(QStringLiteral("Chip Authentication")));
    EXPECT_TRUE(printed.contains(QStringLiteral("Passive Authentication")));
    EXPECT_TRUE(printed.contains(qtTrId("lc-emrtd-csca-not-configured")))
        << "the check that did not run lost its reason";
    EXPECT_FALSE(printed.contains(qtTrId("lc-emrtd-csca-chain-failed")))
        << "a check that passed still spent a paragraph explaining itself";
}

// A read whose security group carries only the three aggregates gets no
// per-check block at all — not an empty heading over nothing.
TEST_F(EmrtdTextDocumentTest, AggregateOnlyReadPrintsNoCheckBlock)
{
    TestableEMRTDTextDocument doc(
        readWith({textField(QStringLiteral("overall_integrity"), QStringLiteral("PASSED")),
                  textField(QStringLiteral("overall_authenticity"), QStringLiteral("NOT_PERFORMED")),
                  textField(QStringLiteral("overall_genuineness"), QStringLiteral("NOT_SUPPORTED"))}));

    const QString printed = doc.text();
    EXPECT_TRUE(printed.contains(qtTrId("lc-emrtd-security-authenticity")))
        << "the aggregate verdicts stopped printing";
    EXPECT_FALSE(printed.contains(qtTrId("lc-emrtd-security-details")))
        << "an empty per-check heading printed over nothing";
}

// The checks arrive indexed, and an index the read never filled leaves a hole.
// A row naming nothing, verdict "Not Performed", is noise a reader would count
// as a check somebody skipped.
TEST_F(EmrtdTextDocumentTest, IndexGapDoesNotPrintANamelessRow)
{
    // Every aggregate is pinned to a DIFFERENT verdict so the one occurrence
    // of "Not Performed" below the fold belongs to a check, not to a summary
    // cell that defaulted there.
    QList<Field> fields{textField(QStringLiteral("overall_integrity"), QStringLiteral("PASSED")),
                        textField(QStringLiteral("overall_authenticity"), QStringLiteral("NOT_PERFORMED")),
                        textField(QStringLiteral("overall_genuineness"), QStringLiteral("NOT_SUPPORTED"))};
    fields +=
        check(0, QStringLiteral("passive_auth"), QStringLiteral("data_authenticity"), QStringLiteral("NOT_PERFORMED"),
              QStringLiteral("Passive Authentication"), QStringLiteral("csca.not-configured"));
    // index 1 deliberately absent
    fields += check(2, QStringLiteral("chip_auth"), QStringLiteral("chip_genuineness"), QStringLiteral("PASSED"),
                    QStringLiteral("Chip Authentication"));
    TestableEMRTDTextDocument doc(readWith(fields));

    const QString printed = doc.text();
    EXPECT_TRUE(printed.contains(QStringLiteral("Passive Authentication")));
    EXPECT_TRUE(printed.contains(QStringLiteral("Chip Authentication")));
    // One aggregate cell plus one check. A row for the empty index 1 would be
    // a third.
    EXPECT_EQ(printed.count(qtTrId("lc-emrtd-security-not-performed")), 2)
        << "a nameless placeholder row printed as a check; printed: " << qPrintable(printed);
}

// A reason this build has never heard of reaches the page verbatim — the same
// rule the pane applies. On paper it is being pasted into markup, so it has to
// arrive as the token it is rather than as formatting.
TEST_F(EmrtdTextDocumentTest, UnknownReasonPrintsAsTextNotMarkup)
{
    const QString future = QStringLiteral("csca.<b>a-reason-from-a-later-build</b>");
    TestableEMRTDTextDocument doc(
        readWith(QList<Field>{textField(QStringLiteral("overall_authenticity"), QStringLiteral("NOT_PERFORMED"))} +
                 check(0, QStringLiteral("passive_auth"), QStringLiteral("data_authenticity"),
                       QStringLiteral("NOT_PERFORMED"), QStringLiteral("Passive Authentication"), future)));

    const QString printed = doc.text();
    EXPECT_TRUE(printed.contains(future))
        << "an unrecognised reason was dropped, or its angle brackets were eaten as markup; printed: "
        << qPrintable(printed);
}

// A read with no security group at all still prints; the whole security block
// leaves with it rather than stranding a heading.
TEST_F(EmrtdTextDocumentTest, ReadWithoutSecurityGroupStillPrints)
{
    QList<FieldGroup> groups;
    groups.append(
        group(QStringLiteral("personal"), {textField(QStringLiteral("surname"), QStringLiteral("PETROVIC"))}));
    TestableEMRTDTextDocument doc(groups);

    const QString printed = doc.text();
    EXPECT_TRUE(printed.contains(QStringLiteral("PETROVIC")));
    EXPECT_FALSE(printed.contains(qtTrId("lc-emrtd-security-details")));
    EXPECT_FALSE(printed.contains(qtTrId("lc-emrtd-security-authenticity")));
}

TEST_F(EmrtdTextDocumentTest, EmptyReadDoesNotCrash)
{
    QList<FieldGroup> empty;
    EXPECT_NO_THROW({ TestableEMRTDTextDocument doc(empty); });
}
