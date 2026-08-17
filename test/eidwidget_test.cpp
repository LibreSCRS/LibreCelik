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
#include "plugins/rs-eid/eidwidget.h"
#include "utils/collapsiblesection.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QLabel>
#include <QList>
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
