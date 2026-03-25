// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include "utils/stringutils.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(CleanAddressTest, TrailingCommasStripped)
{
    EXPECT_EQ(cleanAddress("A,B,C,,"), "A, B, C");
}

TEST(CleanAddressTest, MiddleEmptySegmentsStripped)
{
    EXPECT_EQ(cleanAddress("A,,B"), "A, B");
}

TEST(CleanAddressTest, WhitespaceNormalized)
{
    EXPECT_EQ(cleanAddress("A , B,C"), "A, B, C");
}

TEST(CleanAddressTest, SingleValue)
{
    EXPECT_EQ(cleanAddress("BEOGRAD"), "BEOGRAD");
}

TEST(CleanAddressTest, AllCommasEmpty)
{
    EXPECT_EQ(cleanAddress(",,"), "");
}

TEST(CleanAddressTest, EmptyString)
{
    EXPECT_EQ(cleanAddress(""), "");
}

TEST(CleanAddressTest, RealWorldSerbian)
{
    EXPECT_EQ(cleanAddress("BEOGRAD,NOVI BEOGRAD,BULEVAR MIHAJLA PUPINA,207,,"),
              "BEOGRAD, NOVI BEOGRAD, BULEVAR MIHAJLA PUPINA, 207");
}
