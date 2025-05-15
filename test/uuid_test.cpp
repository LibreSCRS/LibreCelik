// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include "uuid/uuid.h"

class UUIDTest : public ::testing::Test
{
  protected:
    virtual void SetUp()
    {
    }
    
    virtual void TearDown()
    {
    }
};

TEST_F(UUIDTest, UUIDSimple) {
    uuid_t uuid_random;
    uuid_t uuid_time;
    uuid_generate_random(uuid_random);
    uuid_generate_time(uuid_time);

    uuid_string_t outstrRandom;
    uuid_unparse_lower(uuid_random, outstrRandom);
    std::string outStrRandomStd (outstrRandom);

    uuid_string_t outstrTime;
    uuid_unparse(uuid_time, outstrTime);
    std::string outStrTimeStd (outstrTime);

    EXPECT_NE(uuid_random, uuid_time);
    EXPECT_TRUE(outStrRandomStd != outStrTimeStd);
}
