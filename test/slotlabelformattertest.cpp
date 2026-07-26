// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QString>
#include <QTranslator>

#include "signing/slotlabelformatter.h"

using librecelik::signing::formatSlotLabel;

namespace {

// QCoreApplication + QTranslator are required for qtTrId() lookups.
// Without a translator, qtTrId returns the catalog ID string
// (e.g. "lc-pin-label-auth") rather than the source English text,
// so we install LibreCelik_en.qm at suite setup. The .qm path is
// baked at compile time via LIBRECELIK_TRANSLATIONS_DIR_DEFAULT (see
// test/CMakeLists.txt) — same pattern as i18n_settings_state_test.
int dummyArgc = 1;
char dummyArgv0[] = "slotlabelformattertest";
char* dummyArgv[] = {dummyArgv0, nullptr};
QCoreApplication* g_app = nullptr;
QTranslator* g_translator = nullptr;

class SlotLabelFormatterTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QCoreApplication::instance())
            g_app = new QCoreApplication(dummyArgc, dummyArgv);

        g_translator = new QTranslator();
        const QString qmDir = QStringLiteral(LIBRECELIK_TRANSLATIONS_DIR_DEFAULT);
        ASSERT_TRUE(g_translator->load(QStringLiteral("LibreCelik_en"), qmDir))
            << "failed to load LibreCelik_en.qm from " << qmDir.toStdString();
        QCoreApplication::installTranslator(g_translator);
    }
};

} // namespace

TEST_F(SlotLabelFormatterTest, KnownAuth)
{
    auto out = formatSlotLabel(QStringLiteral("Sample eID"), QStringLiteral("Authentication"));
    EXPECT_EQ(out, QStringLiteral("Sample eID — Authentication"));
}

TEST_F(SlotLabelFormatterTest, KnownQscd)
{
    auto out = formatSlotLabel(QStringLiteral("Sample eID"), QStringLiteral("Signing (QSCD)"));
    EXPECT_EQ(out, QStringLiteral("Sample eID — Signing (QSCD)"));
}

TEST_F(SlotLabelFormatterTest, KnownSign)
{
    auto out = formatSlotLabel(QStringLiteral("Token"), QStringLiteral("Signing"));
    EXPECT_EQ(out, QStringLiteral("Token — Signing"));
}

TEST_F(SlotLabelFormatterTest, UnknownPassThrough)
{
    auto out = formatSlotLabel(QStringLiteral("Card"), QStringLiteral("Custom-PIN"));
    EXPECT_EQ(out, QStringLiteral("Card — Custom-PIN"));
}

TEST_F(SlotLabelFormatterTest, EmptyTokenLabelOmitsSeparator)
{
    auto out = formatSlotLabel(QString{}, QStringLiteral("Authentication"));
    EXPECT_EQ(out, QStringLiteral("Authentication"));
}
