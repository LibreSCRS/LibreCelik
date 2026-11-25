// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Offscreen rendering coverage for EidWidget's full-model ctor.
///
/// The full-model ctor is what a card page is (re)built from at
/// identityReady, and the model's group order is DELIVERY-DEPENDENT: a
/// streamed read hands over stream order, while a recovered (instant) read
/// hands over the wire map's keyed order with the merged photo group last.
/// The widget's sections chain — "meta" raises the shell, "personal" hangs
/// the photo row and the badge host off it, everything else attaches to
/// those — so the ctor must stage the model into its own build order. These
/// cases pin that every section renders REGARDLESS of arrival order; before
/// the staging existed, the recovered order silently dropped the address and
/// document sections, and worse orders dropped everything but the shell.

#include "i18n_test_support/mock_plugin_data.h"
#include "rs-eid/eidwidget.h"
#include "utils/collapsiblesection.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

using librecelik::test::i18n::mock::makeEidMock;
using librecelik::test::i18n::mock::detail::group;
using librecelik::test::i18n::mock::detail::textField;
using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

namespace {

/// The shared offscreen application host — same guard as the main-flow
/// suite: widgets abort without a QApplication under the offscreen platform.
class EidWidgetTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QApplication::instance()) {
            // Static: QApplication keeps a reference to argc for its lifetime.
            static int argc = 0;
            app = new QApplication(argc, nullptr);
        }
    }
    static QApplication* app;
};
QApplication* EidWidgetTest::app = nullptr;

/// Deliberately NOT the 240x320 placeholder, so a rendered portrait is
/// distinguishable from the placeholder by size alone.
const QSize kPhotoSize(2, 3);

QByteArray tinyPngBytes()
{
    QImage image(kPhotoSize, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

FieldGroup photoGroup()
{
    Field field;
    field.key = QStringLiteral("photo");
    field.detail = tinyPngBytes();
    return group(QStringLiteral("photo"), {field});
}

FieldGroup verificationGroup()
{
    return group(
        QStringLiteral("verification"),
        {
            textField(QStringLiteral("card_verification"), QStringLiteral("Card"), QStringLiteral("valid")),
            textField(QStringLiteral("fixed_verification"), QStringLiteral("Fixed"), QStringLiteral("valid")),
            textField(QStringLiteral("variable_verification"), QStringLiteral("Variable"), QStringLiteral("invalid")),
        });
}

/// The six-group model of a photo-carrying eID read, ordered by @p keys.
QList<FieldGroup> modelInOrder(const QStringList& keys)
{
    QList<FieldGroup> all = makeEidMock();
    all.append(verificationGroup());
    all.append(photoGroup());

    QList<FieldGroup> ordered;
    for (const QString& key : keys) {
        for (const FieldGroup& candidate : all) {
            if (candidate.key == key)
                ordered.append(candidate);
        }
    }
    EXPECT_EQ(ordered.size(), all.size()) << "order list must name every mock group exactly once";
    return ordered;
}

void expectFullyRendered(EidWidget& widget)
{
    // Outer shell + personal + address + document.
    const auto sections = widget.findChildren<CollapsibleSection*>();
    EXPECT_EQ(sections.size(), 4) << "a full model must render the shell and all three data sections";

    // The portrait: exactly one label carries the decoded photo pixmap, and
    // none still carries the 240x320 placeholder.
    int portraits = 0;
    int placeholders = 0;
    const auto labels = widget.findChildren<QLabel*>();
    for (const QLabel* label : labels) {
        const QPixmap pixmap = label->pixmap();
        if (pixmap.size() == kPhotoSize)
            ++portraits;
        if (pixmap.size() == QSize(240, 320))
            ++placeholders;
    }
    EXPECT_EQ(portraits, 1) << "the decoded portrait must be rendered";
    EXPECT_EQ(placeholders, 0) << "no label may keep the placeholder once the photo group is in the model";

    // The badges: one container, scripted verdicts (2 valid, 1 invalid), no
    // Unknown left behind.
    const auto badges = widget.findChildren<QWidget*>(QStringLiteral("verificationBadge"));
    ASSERT_EQ(badges.size(), 1) << "exactly one badge container must survive the verification group";
    int valid = 0;
    int invalid = 0;
    int unknown = 0;
    for (const QLabel* label : badges.first()->findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("✔"))
            ++valid;
        if (label->text() == QStringLiteral("✘"))
            ++invalid;
        if (label->text() == QStringLiteral("?"))
            ++unknown;
    }
    EXPECT_EQ(valid, 2);
    EXPECT_EQ(invalid, 1);
    EXPECT_EQ(unknown, 0) << "verification data in the model must not render Unknown badges";
}

} // namespace

TEST_F(EidWidgetTest, FinalModelInRecoveredWireOrderRendersEverySection)
{
    // The measured order a recovered (instant, cache-hit) read delivers:
    // the wire map's keyed order, merged photo last.
    EidWidget widget(
        modelInOrder({QStringLiteral("address"), QStringLiteral("document"), QStringLiteral("meta"),
                      QStringLiteral("personal"), QStringLiteral("verification"), QStringLiteral("photo")}));
    expectFullyRendered(widget);
}

TEST_F(EidWidgetTest, FinalModelInReversedStageOrderRendersEverySection)
{
    // Worst-case hardening: every group arrives before the group it hangs
    // off. Arrival order is a wire artifact, never a layout contract.
    EidWidget widget(modelInOrder({QStringLiteral("verification"), QStringLiteral("photo"), QStringLiteral("document"),
                                   QStringLiteral("address"), QStringLiteral("personal"), QStringLiteral("meta")}));
    expectFullyRendered(widget);
}

namespace {

FieldGroup metaGroup(const QString& code)
{
    return group(QStringLiteral("meta"), {textField(QStringLiteral("card_type"), QStringLiteral("Type"), code)});
}

FieldGroup fieldsGroup(const QString& key, const QList<QPair<QString, QString>>& fields)
{
    QList<Field> built;
    for (const auto& [fieldKey, value] : fields)
        built.append(textField(fieldKey, fieldKey, value));
    return group(key, built);
}

/// Without a catalogue qtTrId() hands back the bare id, so the section
/// titles and the visible row labels read as the literal ids under test.
struct Visible
{
    QSet<QString> titles;
    QSet<QString> labels;
    QSet<QString> values;
};

Visible visibleIn(EidWidget& widget)
{
    Visible out;
    for (const CollapsibleSection* section : widget.findChildren<CollapsibleSection*>())
        out.titles.insert(section->title());
    for (const QLabel* label : widget.findChildren<QLabel*>()) {
        if (label->text().startsWith(QLatin1String("lc-eid-label-")))
            out.labels.insert(label->text());
    }
    for (const QLineEdit* edit : widget.findChildren<QLineEdit*>())
        out.values.insert(edit->text());
    return out;
}

QList<FieldGroup> personalDocumentModel(const QString& code, const QList<QPair<QString, QString>>& personal,
                                        const QList<QPair<QString, QString>>& document)
{
    return {metaGroup(code), fieldsGroup(QStringLiteral("personal"), personal),
            fieldsGroup(QStringLiteral("document"), document)};
}

} // namespace

TEST_F(EidWidgetTest, SectionsPerDocument)
{
    using P = QPair<QString, QString>;
    const QList<P> address = {{QStringLiteral("place"), QStringLiteral("Beograd")}};

    // ID: citizen layout, parent name and JMBG, address present.
    {
        auto groups = personalDocumentModel(QStringLiteral("ID"),
                                            {{QStringLiteral("surname"), QStringLiteral("PETROVIĆ")},
                                             {QStringLiteral("parent_given_name"), QStringLiteral("IVAN")},
                                             {QStringLiteral("personal_number"), QStringLiteral("0101990710123")},
                                             {QStringLiteral("state_of_birth_code"), QStringLiteral("SRB")}},
                                            {{QStringLiteral("doc_reg_no"), QStringLiteral("006953897")}});
        groups.append(fieldsGroup(QStringLiteral("address"), address));
        EidWidget widget(groups);
        const Visible v = visibleIn(widget);
        EXPECT_TRUE(v.titles.contains(QStringLiteral("lc-eid-title")));
        EXPECT_TRUE(v.titles.contains(QStringLiteral("lc-eid-label-address")));
        EXPECT_FALSE(v.titles.contains(QStringLiteral("lc-eid-service-title")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-parent-name")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-jmbg")));
        EXPECT_FALSE(v.values.contains(QStringLiteral("SRB"))) << "state_of_birth_code is never shown";
    }
    // IF: foreigner layout, card number, no permit fields.
    {
        auto groups = personalDocumentModel(QStringLiteral("IF"),
                                            {{QStringLiteral("personal_number"), QStringLiteral("1234567")},
                                             {QStringLiteral("nationality"), QStringLiteral("UTOPIJA")},
                                             {QStringLiteral("state_of_birth_code"), QStringLiteral("UTO")}},
                                            {{QStringLiteral("card_number"), QStringLiteral("LS00036515")}});
        groups.append(fieldsGroup(QStringLiteral("address"), address));
        EidWidget widget(groups);
        const Visible v = visibleIn(widget);
        EXPECT_TRUE(v.titles.contains(QStringLiteral("lc-eid-doc-title-foreigner")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-ebs")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-nationality")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-card-number")));
        EXPECT_FALSE(v.labels.contains(QStringLiteral("lc-eid-label-parent-name")));
        EXPECT_FALSE(v.labels.contains(QStringLiteral("lc-eid-label-purpose-of-stay")))
            << "permit fields belong to the RP layout, whatever the card sends";
        EXPECT_FALSE(v.values.contains(QStringLiteral("STRAY PURPOSE")));
        EXPECT_FALSE(v.values.contains(QStringLiteral("UTO")));
    }
    // RP: foreigner layout plus permit name, purpose, note and card number.
    {
        auto groups = personalDocumentModel(
            QStringLiteral("RP"),
            {{QStringLiteral("personal_number"), QStringLiteral("1234567")},
             {QStringLiteral("purpose_of_stay"), QStringLiteral("KRETANJE U OKVIRU PRIVREDNOG DRUŠTVA")},
             {QStringLiteral("note"), QStringLiteral("JEDINSTVENA DOZVOLA")}},
            {{QStringLiteral("document_name"), QStringLiteral("DOZVOLA ZA PRIVREMENI BORAVAK I RAD")},
             {QStringLiteral("card_number"), QStringLiteral("BD00169609")}});
        groups.append(fieldsGroup(QStringLiteral("address"), address));
        EidWidget widget(groups);
        const Visible v = visibleIn(widget);
        EXPECT_TRUE(v.titles.contains(QStringLiteral("lc-eid-doc-title-residence-permit")));
        EXPECT_FALSE(v.titles.contains(QStringLiteral("lc-eid-title")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-ebs")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-purpose-of-stay")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-note")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-document-name")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-card-number")));
        EXPECT_TRUE(v.values.contains(QStringLiteral("JEDINSTVENA DOZVOLA")));
    }
    // EL: blood group, service section, no address section.
    {
        auto groups = personalDocumentModel(QStringLiteral("EL"),
                                            {{QStringLiteral("personal_number"), QStringLiteral("3107972777016")},
                                             {QStringLiteral("blood_group"), QStringLiteral("A-")}},
                                            {{QStringLiteral("doc_reg_no"), QStringLiteral("400000097")}});
        groups.append(fieldsGroup(QStringLiteral("service"), {{QStringLiteral("status"), QStringLiteral("1")},
                                                              {QStringLiteral("badge_number"), QStringLiteral("97")},
                                                              {QStringLiteral("rank"), QStringLiteral("Načelnik")}}));
        EidWidget widget(groups);
        const Visible v = visibleIn(widget);
        EXPECT_TRUE(v.titles.contains(QStringLiteral("lc-eid-doc-title-service-id")));
        EXPECT_FALSE(v.titles.contains(QStringLiteral("lc-eid-title")));
        EXPECT_TRUE(v.titles.contains(QStringLiteral("lc-eid-service-title")));
        EXPECT_FALSE(v.titles.contains(QStringLiteral("lc-eid-label-address")));
        EXPECT_FALSE(v.titles.contains(QStringLiteral("lc-eid-label-address-foreigner")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-blood-group")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-status")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-badge-number")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-rank")));
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-jmbg")));
        EXPECT_FALSE(v.labels.contains(QStringLiteral("lc-eid-label-parent-name")));
    }
    // EL given an address group anyway: the layout has no address block.
    {
        auto groups =
            personalDocumentModel(QStringLiteral("EL"), {{QStringLiteral("blood_group"), QStringLiteral("A-")}},
                                  {{QStringLiteral("doc_reg_no"), QStringLiteral("400000097")}});
        groups.append(fieldsGroup(QStringLiteral("address"), address));
        EidWidget widget(groups);
        const Visible v = visibleIn(widget);
        EXPECT_FALSE(v.titles.contains(QStringLiteral("lc-eid-label-address")));
        EXPECT_FALSE(v.values.contains(QStringLiteral("Beograd")));
    }
}

TEST_F(EidWidgetTest, UnknownCodeFallsBackToCitizenLayout)
{
    for (const QString code : {QStringLiteral("XX"), QStringLiteral("Citizen2014"), QString()}) {
        auto groups = personalDocumentModel(code,
                                            {{QStringLiteral("parent_given_name"), QStringLiteral("IVAN")},
                                             {QStringLiteral("personal_number"), QStringLiteral("0101990710123")},
                                             {QStringLiteral("nationality"), QStringLiteral("UTOPIJA")}},
                                            {{QStringLiteral("doc_reg_no"), QStringLiteral("006953897")}});
        EidWidget widget(groups);
        const Visible v = visibleIn(widget);
        EXPECT_TRUE(v.titles.contains(QStringLiteral("lc-eid-title"))) << code.toStdString();
        EXPECT_FALSE(v.titles.contains(QStringLiteral("lc-eid-doc-title-foreigner"))) << code.toStdString();
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-parent-name"))) << code.toStdString();
        EXPECT_TRUE(v.labels.contains(QStringLiteral("lc-eid-label-jmbg"))) << code.toStdString();
        EXPECT_FALSE(v.labels.contains(QStringLiteral("lc-eid-label-nationality"))) << code.toStdString();
    }
}
