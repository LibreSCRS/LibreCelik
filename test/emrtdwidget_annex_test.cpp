// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The annex sections, and the verdict that has to travel WITH them.
///
/// A card may carry a national annex beside its travel-document data: a signed
/// container of extra personal detail, read only after its own signature
/// verifies. It reaches this widget as two groups — `annex.<id>.personal` and
/// `annex.<id>.security` — whose keys are DERIVED from the annex's id so that
/// two annexes on one card cannot collide.
///
/// Two properties these cases exist to hold:
///
///  1. The verdict renders INSIDE the section it describes. Two verdicts on one
///     screen — the travel document's passive authentication and the annex's
///     own, weaker one — read as a single guarantee when they sit in one flat
///     list, and a reader then credits the annex with a check nobody ran.
///     So the assertions descend from the SECTION, never from the widget: a
///     verdict glued anywhere at all would satisfy a widget-rooted search.
///
///  2. Arrival order is not a contract. A streamed read delivers emission
///     order; a recovered one delivers the wire map's keyed order. Both must
///     land the verdict in the same place.

#include "emrtd/emrtdwidget.h"
#include "utils/collapsiblesection.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTranslator>

#include <gtest/gtest.h>

#include <algorithm>
#include <utility>
#include <vector>

using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

namespace {

Field textField(const QString& key, const QString& label, const QString& value)
{
    Field f;
    f.key = key;
    f.value = value;
    f.extra.insert(QStringLiteral("labelFallback"), label);
    f.extra.insert(QStringLiteral("type"), QStringLiteral("text"));
    return f;
}

FieldGroup group(const QString& key, QList<Field> fields)
{
    FieldGroup g;
    g.key = key;
    g.fields = std::move(fields);
    return g;
}

/// The verdict group the annex reader emits: integrity proven, authenticity
/// deliberately NOT claimed (no trust anchor is offered for an annex yet).
FieldGroup annexSecurity(const QString& id)
{
    return group(
        QStringLiteral("annex.%1.security").arg(id),
        {textField(QStringLiteral("annex_integrity"), QStringLiteral("Data Integrity"), QStringLiteral("PASSED")),
         textField(QStringLiteral("annex_authenticity"), QStringLiteral("Data Authenticity"),
                   QStringLiteral("NOT_PERFORMED"))});
}

/// A minimal personal annex group, with a value that is easy to find again.
FieldGroup annexPersonal(const QString& id, const QString& street = QStringLiteral("Kneza Milosa"))
{
    return group(QStringLiteral("annex.%1.personal").arg(id),
                 {textField(QStringLiteral("street"), QStringLiteral("Street"), street),
                  textField(QStringLiteral("place"), QStringLiteral("Place"), QStringLiteral("Beograd"))});
}

/// Every section this widget has built, by title.
QStringList sectionTitles(const QWidget& widget)
{
    QStringList titles;
    for (const CollapsibleSection* section : widget.findChildren<CollapsibleSection*>()) {
        titles << section->title();
    }
    return titles;
}

/// The section whose title matches @p title, or nullptr.
const CollapsibleSection* sectionTitled(const QWidget& widget, const QString& title)
{
    for (const CollapsibleSection* section : widget.findChildren<CollapsibleSection*>()) {
        if (section->title() == title) {
            return section;
        }
    }
    return nullptr;
}

/// Every string rendered as a VALUE inside @p root (the read-only line edits
/// the field grid builds), in layout order.
QStringList valuesUnder(const QWidget& root)
{
    QStringList values;
    for (const QLineEdit* edit : root.findChildren<QLineEdit*>()) {
        values << edit->text();
    }
    return values;
}

/// Every label string rendered under @p root.
QStringList labelsUnder(const QWidget& root)
{
    QStringList labels;
    for (const QLabel* label : root.findChildren<QLabel*>()) {
        if (!label->text().isEmpty()) {
            labels << label->text();
        }
    }
    return labels;
}

class EmrtdAnnexTest : public ::testing::Test
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
        // Without a catalog, qtTrId() returns the bare id — and ids are unique
        // by construction, so the title-collision case could never see the
        // collision it exists to catch. It has to compare what a reader reads.
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
QApplication* EmrtdAnnexTest::app = nullptr;
QTranslator* EmrtdAnnexTest::translator = nullptr;

} // namespace

// --- the gap this work closes -----------------------------------------------

// addGroup's chain named twelve keys and had no final else, so both annex
// groups fell through it and vanished: fifteen fields read off the card and
// verified against its signature, dropped by the client that asked for them.
TEST_F(EmrtdAnnexTest, AnnexPersonalGroupRendersItsOwnSection)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(annexPersonal(QStringLiteral("rs")));

    const CollapsibleSection* section = sectionTitled(widget, qtTrId("lc-annex-additional-data"));
    ASSERT_NE(section, nullptr) << "sections built: " << qPrintable(sectionTitles(widget).join(u", "));
    EXPECT_TRUE(valuesUnder(*section).contains(QStringLiteral("Kneza Milosa")));
}

// --- the verdict travels with its data --------------------------------------

TEST_F(EmrtdAnnexTest, AnnexSecurityRendersInsideTheAnnexSection)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(annexPersonal(QStringLiteral("rs")));
    widget.addGroup(annexSecurity(QStringLiteral("rs")));

    const CollapsibleSection* section = sectionTitled(widget, qtTrId("lc-annex-additional-data"));
    ASSERT_NE(section, nullptr);

    // Rooted at the SECTION, deliberately: a verdict appended anywhere in the
    // widget would pass a widget-rooted search while still telling the reader
    // nothing about WHICH data it covers.
    const QStringList text = labelsUnder(*section) + valuesUnder(*section);
    EXPECT_TRUE(text.join(u'\n').contains(qtTrId("lc-annex-integrity")))
        << "verdict not found inside the annex section: " << qPrintable(text.join(u" | "));
    EXPECT_TRUE(text.join(u'\n').contains(qtTrId("lc-annex-authenticity")));
}

// The wire delivers the verdict as a key-sorted map, so "annex_authenticity"
// arrives before "annex_integrity". The pane must still render integrity first
// — matching the travel document's own order — and under a scoped heading, so a
// reader never credits the annex badges to the passport.
TEST_F(EmrtdAnnexTest, AnnexVerdictRendersIntegrityFirstUnderItsOwnHeading)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(annexPersonal(QStringLiteral("rs")));
    // Wire order: authenticity BEFORE integrity (alphabetical by key).
    widget.addGroup(group(
        QStringLiteral("annex.rs.security"),
        {textField(QStringLiteral("annex_authenticity"), QStringLiteral("Data Authenticity"),
                   QStringLiteral("NOT_PERFORMED")),
         textField(QStringLiteral("annex_integrity"), QStringLiteral("Data Integrity"), QStringLiteral("PASSED"))}));

    const CollapsibleSection* section = sectionTitled(widget, qtTrId("lc-annex-additional-data"));
    ASSERT_NE(section, nullptr);
    const QStringList labels = labelsUnder(*section);

    // The status rows render as "<label>: <status>", so match by prefix.
    const auto indexStarting = [&labels](const QString& prefix) -> qsizetype {
        for (qsizetype i = 0; i < labels.size(); ++i) {
            if (labels.at(i).startsWith(prefix)) {
                return i;
            }
        }
        return -1;
    };
    const qsizetype heading = labels.indexOf(qtTrId("lc-annex-verification"));
    const qsizetype integrity = indexStarting(qtTrId("lc-annex-integrity"));
    const qsizetype authenticity = indexStarting(qtTrId("lc-annex-authenticity"));
    ASSERT_GE(heading, 0) << "the annex verdict must sit under its own scoped heading";
    ASSERT_GE(integrity, 0);
    ASSERT_GE(authenticity, 0);
    EXPECT_LT(heading, integrity) << "the heading must precede the verdict rows";
    EXPECT_LT(integrity, authenticity)
        << "integrity must render before authenticity, matching the travel-document pane";
}

// The shared wire vocabulary can grow: a security-group field outside the
// pinned integrity/authenticity pair must still render — after the pair,
// through the labelFallback path — never be silently dropped.
TEST_F(EmrtdAnnexTest, UnknownSecurityFieldStillRendersAfterTheVerdictPair)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(annexPersonal(QStringLiteral("rs")));
    widget.addGroup(group(
        QStringLiteral("annex.rs.security"),
        {textField(QStringLiteral("annex_authenticity"), QStringLiteral("Data Authenticity"),
                   QStringLiteral("NOT_PERFORMED")),
         textField(QStringLiteral("annex_freshness"), QStringLiteral("Data Freshness"), QStringLiteral("PASSED")),
         textField(QStringLiteral("annex_integrity"), QStringLiteral("Data Integrity"), QStringLiteral("PASSED"))}));

    const CollapsibleSection* section = sectionTitled(widget, qtTrId("lc-annex-additional-data"));
    ASSERT_NE(section, nullptr);
    const QStringList labels = labelsUnder(*section);
    const auto indexStarting = [&labels](const QString& prefix) -> qsizetype {
        for (qsizetype i = 0; i < labels.size(); ++i) {
            if (labels.at(i).startsWith(prefix)) {
                return i;
            }
        }
        return -1;
    };
    const qsizetype authenticity = indexStarting(qtTrId("lc-annex-authenticity"));
    const qsizetype freshness = indexStarting(QStringLiteral("Data Freshness"));
    ASSERT_GE(authenticity, 0);
    ASSERT_GE(freshness, 0) << "a field outside the pinned pair must not be dropped";
    EXPECT_LT(authenticity, freshness) << "unpinned fields follow the pinned pair";
}

TEST_F(EmrtdAnnexTest, AnnexSecurityArrivingFirstStillLandsInTheSection)
{
    EMRTDWidget widget(nullptr);
    // Reversed: the verdict arrives before the section that will hold it exists.
    widget.addGroup(annexSecurity(QStringLiteral("rs")));
    widget.addGroup(annexPersonal(QStringLiteral("rs")));

    const CollapsibleSection* section = sectionTitled(widget, qtTrId("lc-annex-additional-data"));
    ASSERT_NE(section, nullptr);
    const QStringList text = labelsUnder(*section) + valuesUnder(*section);
    EXPECT_TRUE(text.join(u'\n').contains(qtTrId("lc-annex-integrity")));
}

// Two annexes, so "keep the pending verdict in one member" cannot pass. The
// target is the MIDDLE one of three ids: with only two, "take the first" and
// "take the last" both succeed by accident.
TEST_F(EmrtdAnnexTest, TwoAnnexesDoNotShareAVerdict)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(annexPersonal(QStringLiteral("aa"), QStringLiteral("Street AA")));
    widget.addGroup(annexPersonal(QStringLiteral("mm"), QStringLiteral("Street MM")));
    widget.addGroup(annexPersonal(QStringLiteral("zz"), QStringLiteral("Street ZZ")));
    // Only the middle annex reports a verdict.
    widget.addGroup(annexSecurity(QStringLiteral("mm")));

    // Only the annex sections themselves. findChildren is recursive and the
    // widget's outer shell is an ancestor of all three, so a widget-wide scan
    // counts the shell as carrying whatever any child carries.
    QList<const CollapsibleSection*> annexes;
    for (const CollapsibleSection* section : widget.findChildren<CollapsibleSection*>()) {
        if (section->title() == qtTrId("lc-annex-additional-data")) {
            annexes << section;
        }
    }
    ASSERT_EQ(annexes.size(), 3);

    QStringList carryingTheVerdict;
    for (const CollapsibleSection* section : annexes) {
        const QStringList text = labelsUnder(*section) + valuesUnder(*section);
        if (!text.join(u'\n').contains(qtTrId("lc-annex-integrity"))) {
            continue;
        }
        // Name the annex by the street only it carries, so the failure message
        // says WHICH one wrongly received it.
        for (const QString& street :
             {QStringLiteral("Street AA"), QStringLiteral("Street MM"), QStringLiteral("Street ZZ")}) {
            if (text.contains(street)) {
                carryingTheVerdict << street;
            }
        }
    }
    EXPECT_EQ(carryingTheVerdict, QStringList{QStringLiteral("Street MM")})
        << "verdict landed on: " << qPrintable(carryingTheVerdict.join(u", "));
}

// --- the key is a prefix, not a name ----------------------------------------

// The annex id comes from the reader, and this issuer has already moved its
// applet identifier once. Binding the client to `annex.rs.` would make the next
// annex silently vanish exactly as these two did.
TEST_F(EmrtdAnnexTest, UnknownAnnexIdStillRenders)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(annexPersonal(QStringLiteral("zz"), QStringLiteral("Somewhere")));

    const CollapsibleSection* section = sectionTitled(widget, qtTrId("lc-annex-additional-data"));
    ASSERT_NE(section, nullptr);
    EXPECT_TRUE(valuesUnder(*section).contains(QStringLiteral("Somewhere")));
}

// The group key is foreign input as far as this widget is concerned; a
// malformed one must be ignored, not half-parsed into a section with no id.
TEST_F(EmrtdAnnexTest, MalformedAnnexKeyIsIgnored)
{
    for (const QString& key : {QStringLiteral("annex."), QStringLiteral("annex.rs."), QStringLiteral("annex..personal"),
                               QStringLiteral("annex")}) {
        EMRTDWidget widget(nullptr);
        widget.addGroup(
            group(key, {textField(QStringLiteral("street"), QStringLiteral("Street"), QStringLiteral("Nowhere"))}));
        EXPECT_EQ(sectionTitled(widget, qtTrId("lc-annex-additional-data")), nullptr)
            << "malformed key built a section: " << qPrintable(key);
    }
}

// Personal detail that could not be attributed to an issuer is withheld
// entirely rather than shown with a badge — the middleware emits ZERO groups on
// a failed verification. A verdict with no data behind it is therefore never a
// real delivery, and must not raise an empty section.
TEST_F(EmrtdAnnexTest, AnnexSecurityAloneRendersNothing)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(annexSecurity(QStringLiteral("rs")));

    EXPECT_EQ(sectionTitled(widget, qtTrId("lc-annex-additional-data")), nullptr);
}

// --- reading order --------------------------------------------------------

// The wire carries identity as map-of-maps, so fields arrive in LEXICOGRAPHIC
// key order, not the order the reader wrote them. An address rendered that way
// puts "Street" last and "Apartment" third.
//
// The group below is therefore built in ALPHABETICAL order on purpose: built in
// address order it would pass without a line of implementation, proving only
// that a list already sorted stays sorted.
TEST_F(EmrtdAnnexTest, AnnexFieldsFollowAddressOrderNotAlphabet)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(group(
        QStringLiteral("annex.rs.personal"),
        {textField(QStringLiteral("apartment_number"), QStringLiteral("Apartment"), QStringLiteral("V-APARTMENT")),
         textField(QStringLiteral("house_number"), QStringLiteral("House Number"), QStringLiteral("V-HOUSENUMBER")),
         textField(QStringLiteral("place"), QStringLiteral("Place"), QStringLiteral("V-PLACE")),
         textField(QStringLiteral("street"), QStringLiteral("Street"), QStringLiteral("V-STREET"))}));

    const CollapsibleSection* section = sectionTitled(widget, qtTrId("lc-annex-additional-data"));
    ASSERT_NE(section, nullptr);
    const QStringList values = valuesUnder(*section);

    // street is the discriminating target: LAST alphabetically, SECOND by
    // address. Neither "keep as given" nor "sort" produces this order.
    const QStringList expected{QStringLiteral("V-STREET"), QStringLiteral("V-HOUSENUMBER"),
                               QStringLiteral("V-APARTMENT"), QStringLiteral("V-PLACE")};
    EXPECT_EQ(values, expected);
}

// The full 15-key reading order, pinned end to end: fields arrive in the wire
// map's alphabetical order and must render in address-reading order. The list
// is byte-identical to the KDE client's copy (LibreKDE,
// shared/agentclient/IdentityRows.cpp, fieldOrderForGroup()); no shared
// library links the two repositories, so each pins its own copy — change both
// together.
TEST_F(EmrtdAnnexTest, AnnexReadingOrderIsPinnedInFull)
{
    const QStringList readingOrder{
        QStringLiteral("address_label"),     QStringLiteral("street"),
        QStringLiteral("house_number"),      QStringLiteral("house_letter"),
        QStringLiteral("entrance"),          QStringLiteral("floor"),
        QStringLiteral("apartment_number"),  QStringLiteral("place"),
        QStringLiteral("community"),         QStringLiteral("state"),
        QStringLiteral("parent_given_name"), QStringLiteral("community_of_birth"),
        QStringLiteral("state_of_birth"),    QStringLiteral("document_serial"),
        QStringLiteral("address_date"),
    };

    // Delivery deliberately differs from the assertion target, so a reading
    // list that ever degrades to plain key-sorting cannot pass vacuously.
    QStringList wireOrder = readingOrder;
    std::sort(wireOrder.begin(), wireOrder.end());
    ASSERT_NE(wireOrder, readingOrder);

    QList<Field> fields;
    for (const QString& key : wireOrder) {
        fields.append(textField(key, key, QStringLiteral("V-") + key));
    }
    EMRTDWidget widget(nullptr);
    widget.addGroup(group(QStringLiteral("annex.rs.personal"), fields));

    const CollapsibleSection* section = sectionTitled(widget, qtTrId("lc-annex-additional-data"));
    ASSERT_NE(section, nullptr);

    QStringList expectedValues;
    for (const QString& key : readingOrder) {
        expectedValues << QStringLiteral("V-") + key;
    }
    EXPECT_EQ(valuesUnder(*section), expectedValues);
}

// The annex reader ships the address-change date as the card's raw ddMMyyyy
// digits; the widget normalises them to dd.MM.yyyy for display (the middleware
// never reformats signed card bytes).
TEST_F(EmrtdAnnexTest, AddressDateRawDigitsRenderAsFormattedDate)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(
        group(QStringLiteral("annex.rs.personal"),
              {textField(QStringLiteral("address_date"), QStringLiteral("Address Date"), QStringLiteral("06082016"))}));

    const CollapsibleSection* section = sectionTitled(widget, qtTrId("lc-annex-additional-data"));
    ASSERT_NE(section, nullptr);
    const QStringList values = valuesUnder(*section);
    ASSERT_EQ(values.size(), 1);
    EXPECT_EQ(values.first(), QStringLiteral("06.08.2016"));
}

// --- the labels are the point of the exercise -------------------------------

// A key with no entry falls back to the plugin's English label, which is the
// very complaint this work answers. Counting is what catches an entry dropped
// later; naming one string is what catches a table that was never wired up.
TEST_F(EmrtdAnnexTest, AnnexLabelMapCoversEveryKnownField)
{
    const std::map<QString, QString> map = EMRTDWidget::annexTranslationMapForTest();
    EXPECT_EQ(map.size(), 17u) << "15 personal fields + 2 verdict fields";
    for (const QString& key :
         {QStringLiteral("document_serial"), QStringLiteral("parent_given_name"), QStringLiteral("community_of_birth"),
          QStringLiteral("state_of_birth"), QStringLiteral("state"), QStringLiteral("community"),
          QStringLiteral("place"), QStringLiteral("street"), QStringLiteral("house_number"),
          QStringLiteral("house_letter"), QStringLiteral("entrance"), QStringLiteral("floor"),
          QStringLiteral("apartment_number"), QStringLiteral("address_date"), QStringLiteral("address_label"),
          QStringLiteral("annex_integrity"), QStringLiteral("annex_authenticity")}) {
        EXPECT_NE(map.find(key), map.end()) << "unmapped annex field: " << qPrintable(key);
    }
}

// --- the other half of item 149 ---------------------------------------------

// The travel document's own security pane sat under a title that named no
// subject ("Security Status"), so the annex verdict beside it looked like more
// of the same evaluation. The title now says what the pane covers.
TEST_F(EmrtdAnnexTest, SecurityPaneTitleNamesWhatItCovers)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(group(
        QStringLiteral("security_status"),
        {textField(QStringLiteral("overall_integrity"), QStringLiteral("Data Integrity"), QStringLiteral("PASSED"))}));

    EXPECT_NE(sectionTitled(widget, qtTrId("lc-emrtd-security-status-travel-doc")), nullptr)
        << "sections built: " << qPrintable(sectionTitles(widget).join(u", "));
}

// Found on a live card, not by this suite: the annex section and the eMRTD
// DG11 "additional" section were both titled "Additional Data", and a Serbian
// identity card carries BOTH groups — so the page showed two identically named
// sections holding different fields.
//
// Every case above asks whether its own section exists. None asked whether the
// page's sections can be told apart, which is the property a reader actually
// needs.
TEST_F(EmrtdAnnexTest, EverySectionOnOnePageHasItsOwnTitle)
{
    EMRTDWidget widget(nullptr);
    // The two that collided, plus enough neighbours to keep the check honest.
    widget.addGroup(group(QStringLiteral("additional"), {textField(QStringLiteral("address"), QStringLiteral("Address"),
                                                                   QStringLiteral("A free-form address"))}));
    widget.addGroup(group(
        QStringLiteral("document_extra"),
        {textField(QStringLiteral("issuing_authority"), QStringLiteral("Issuing Authority"), QStringLiteral("PU"))}));
    widget.addGroup(group(QStringLiteral("national"),
                          {textField(QStringLiteral("tag"), QStringLiteral("Tag"), QStringLiteral("T"))}));
    widget.addGroup(annexPersonal(QStringLiteral("rs")));

    QStringList titles = sectionTitles(widget);
    titles.removeAll(QString()); // untitled containers are not sections a reader reads
    QStringList unique = titles;
    unique.removeDuplicates();
    EXPECT_EQ(titles.size(), unique.size()) << "two sections share a title: " << qPrintable(titles.join(u" | "));
}

// --- the signer verdict says WHY, and the pane has to say it in words --------

namespace {

/// The travel document's security group carrying one check whose verdict
/// arrived with a machine-readable reason.
///
/// The reason is a key, not a sentence: the reader that judges the signer emits
/// the same token in every language, and turning it into text is this host's
/// job. Anything the host does not translate is text the holder reads as a
/// token.
FieldGroup signerCheck(const QString& status, const QString& reasonKey)
{
    QList<Field> fields{
        textField(QStringLiteral("overall_authenticity"), QStringLiteral("Data Authenticity"), status),
        textField(QStringLiteral("check_0_id"), QStringLiteral("id"), QStringLiteral("passive_auth")),
        textField(QStringLiteral("check_0_category"), QStringLiteral("category"), QStringLiteral("data_authenticity")),
        textField(QStringLiteral("check_0_status"), QStringLiteral("status"), status),
        textField(QStringLiteral("check_0_label"), QStringLiteral("label"), QStringLiteral("Passive Authentication")),
    };
    if (!reasonKey.isEmpty()) {
        fields.append(textField(QStringLiteral("check_0_reason"), QStringLiteral("reason"), reasonKey));
    }
    return group(QStringLiteral("security_status"), fields);
}

/// Every string the security pane renders, for a read whose signer verdict
/// carried @p reasonKey.
QStringList paneTextFor(const EMRTDWidget& widget)
{
    const CollapsibleSection* pane = sectionTitled(widget, qtTrId("lc-emrtd-security-status-travel-doc"));
    return pane != nullptr ? labelsUnder(*pane) : QStringList{};
}

} // namespace

// Each of the five keys the signer check can carry has to reach the pane as a
// sentence. A key with no arm renders as itself, which is the bug this covers:
// "csca.not-configured" is not something a holder can act on.
TEST_F(EmrtdAnnexTest, EverySignerReasonKeyRendersAsText)
{
    const std::vector<std::pair<QString, QString>> cases{
        {QStringLiteral("csca.not-configured"), QStringLiteral("lc-emrtd-csca-not-configured")},
        {QStringLiteral("csca.anchors-unreadable"), QStringLiteral("lc-emrtd-csca-anchors-unreadable")},
        {QStringLiteral("csca.anchors-undecodable"), QStringLiteral("lc-emrtd-csca-anchors-undecodable")},
        {QStringLiteral("csca.no-anchor-for-issuer"), QStringLiteral("lc-emrtd-csca-no-anchor-for-issuer")},
        {QStringLiteral("csca.chain-failed"), QStringLiteral("lc-emrtd-csca-chain-failed")},
    };

    for (const auto& [reasonKey, catalogId] : cases) {
        // Only the last is an accusation; the other four are states of this
        // installation's own configuration, and the read never got far enough
        // to judge the document.
        const QString status = reasonKey == QStringLiteral("csca.chain-failed") ? QStringLiteral("FAILED")
                                                                                : QStringLiteral("NOT_PERFORMED");
        EMRTDWidget widget(nullptr);
        widget.addGroup(signerCheck(status, reasonKey));

        const QStringList text = paneTextFor(widget);
        const QString expected = qtTrId(catalogId.toUtf8().constData());
        EXPECT_TRUE(text.contains(expected))
            << "reason " << qPrintable(reasonKey)
            << " did not reach the pane as text; rendered: " << qPrintable(text.join(u" | "));
        EXPECT_FALSE(text.contains(reasonKey))
            << "reason " << qPrintable(reasonKey) << " reached the holder as a raw key";
        // Load-bearing, and the trap every catalogue test here can walk into:
        // qtTrId() answers with the BARE ID when no <message> carries it, and
        // this pane renders whatever qtTrId() returned. So the EXPECT_TRUE
        // above compares the id against itself the moment the entry is
        // deleted, and passes on a build that shows the holder
        // "lc-emrtd-csca-not-configured". Naming the id as something that must
        // NOT reach the screen is the half that fails. Measured: deleting one
        // <message> from LibreCelik_en.ts leaves the EXPECT_TRUE green.
        EXPECT_FALSE(text.contains(catalogId))
            << "no catalogue entry for " << qPrintable(catalogId) << "; the id itself reached the holder";
    }
}

// The two configuration faults need DIFFERENT instructions -- an empty store is
// fixed by importing one, an unreadable one by fixing its permissions. One
// shared sentence for both is what having a reason key at all was meant to end.
TEST_F(EmrtdAnnexTest, ConfigurationFaultsGiveDifferentInstructions)
{
    const QString notConfigured = qtTrId("lc-emrtd-csca-not-configured");
    const QString unreadable = qtTrId("lc-emrtd-csca-anchors-unreadable");
    const QString undecodable = qtTrId("lc-emrtd-csca-anchors-undecodable");
    const QString noAnchor = qtTrId("lc-emrtd-csca-no-anchor-for-issuer");
    const QString chainFailed = qtTrId("lc-emrtd-csca-chain-failed");

    QStringList all{notConfigured, unreadable, undecodable, noAnchor, chainFailed};
    for (const QString& s : all) {
        EXPECT_FALSE(s.isEmpty());
        EXPECT_FALSE(s.startsWith(QStringLiteral("lc-emrtd-csca-"))) << "no catalogue entry for " << qPrintable(s);
    }
    QStringList unique = all;
    unique.removeDuplicates();
    EXPECT_EQ(all.size(), unique.size()) << "two reasons share one sentence: " << qPrintable(all.join(u" | "));
}

// A newer reader may name a reason this build has never heard of. The row it
// belongs to is a security verdict; dropping it, or replacing it with the word
// "unknown", both cost the holder the only record that something was wrong.
TEST_F(EmrtdAnnexTest, UnknownSignerReasonDegradesInsteadOfErasingTheRow)
{
    EMRTDWidget widget(nullptr);
    const QString future = QStringLiteral("csca.a-reason-from-a-later-build");
    widget.addGroup(signerCheck(QStringLiteral("NOT_PERFORMED"), future));

    const QStringList text = paneTextFor(widget);
    // The check itself still renders...
    EXPECT_TRUE(text.contains(QStringLiteral("Passive Authentication")))
        << "an unknown reason took the whole check row with it: " << qPrintable(text.join(u" | "));
    // ...and the unrecognised reason survives verbatim rather than vanishing or
    // being flattened to a word that says nothing.
    EXPECT_TRUE(text.contains(future)) << "unknown reason erased; rendered: " << qPrintable(text.join(u" | "));
    EXPECT_FALSE(text.contains(QStringLiteral("unknown"), Qt::CaseInsensitive));
}

// A check with no reason at all is the common case -- every check that passed.
// It must not grow an empty line under it.
TEST_F(EmrtdAnnexTest, CheckWithoutAReasonRendersNoReasonLine)
{
    EMRTDWidget widget(nullptr);
    widget.addGroup(signerCheck(QStringLiteral("PASSED"), QString()));

    const QStringList text = paneTextFor(widget);
    EXPECT_TRUE(text.contains(QStringLiteral("Passive Authentication")));
    for (const QString& s : text) {
        EXPECT_FALSE(s.trimmed().isEmpty());
    }
}
