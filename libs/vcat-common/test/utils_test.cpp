
#include "utils.hpp"
#include <cstdint>
#include <stdexcept>

#include <gtest/gtest.h>

namespace
{

TEST(FitDimensionsToRatio, RejectsInputsThereIsNothingToFitInto)
{
    EXPECT_EQ((fitDimensionsToRatio({0, 10}, 2.0f).width), 0);
    EXPECT_EQ((fitDimensionsToRatio({0, 10}, 2.0f).height), 0);
    EXPECT_EQ((fitDimensionsToRatio({10, 0}, 2.0f).width), 0);
    EXPECT_EQ((fitDimensionsToRatio({-3, 10}, 2.0f).height), 0);
    EXPECT_EQ((fitDimensionsToRatio({10, -3}, 2.0f).width), 0);
}

TEST(FitDimensionsToRatio, RejectsANonPositiveTargetRatio)
{
    EXPECT_EQ((fitDimensionsToRatio({10, 10}, 0.0f).width), 0);
    EXPECT_EQ((fitDimensionsToRatio({10, 10}, 0.0f).height), 0);
    EXPECT_EQ((fitDimensionsToRatio({10, 10}, -1.5f).width), 0);
}

TEST(FitDimensionsToRatio, AnExactlyReachableRatioIsHit)
{
    const Rectangle fitted = fitDimensionsToRatio({24, 80}, 2.0f);

    EXPECT_EQ(fitted.height, 24) << "the exact ratio is reachable, so the full height is used";
    EXPECT_EQ(fitted.width, 48);
}

TEST(FitDimensionsToRatio, PrefersTheLargestGridInsideTheTolerance)
{
    // A 2.0 ratio in 10x15 cells: the exact ratio is only reachable up to 7x14, which beats the
    // bigger but out-of-tolerance 10x15 (25% off) and 8x15 (6.25% off) candidates.
    const Rectangle fitted = fitDimensionsToRatio({10, 15}, 2.0f);

    EXPECT_EQ(fitted.height, 7);
    EXPECT_EQ(fitted.width, 14);
}

TEST(FitDimensionsToRatio, FallsBackToTheLeastWrongRatioWhenNothingIsWithinTolerance)
{
    // 3.3 in 2 rows: 6/2 and 3/1 are both 9.1% off, 7/2 is 6.1% off and wins despite the tiny grid.
    const Rectangle fitted = fitDimensionsToRatio({2, 100}, 3.3f);

    EXPECT_EQ(fitted.height, 2);
    EXPECT_EQ(fitted.width, 7);
}

TEST(FitDimensionsToRatio, FitsAWidescreenVideoIntoAStandardTerminal)
{
    // 16:9 video on an 80x24 terminal: the character grid ratio is 16/9 * CHAR_CELL_ASPECT.
    const float targetRatio = (16.0f / 9.0f) * CHAR_CELL_ASPECT;

    const Rectangle fitted = fitDimensionsToRatio({24, 80}, targetRatio);

    EXPECT_EQ(fitted.height, 22);
    EXPECT_EQ(fitted.width, 78);
    EXPECT_LE(fitted.width, 80) << "the grid must fit the terminal";
    EXPECT_LE(fitted.height, 24) << "the grid must fit the terminal";
}

TEST(Ip, AddressPacksTheOctetsMostSignificantFirst)
{
    EXPECT_EQ((Ip{127, 0, 0, 1}.address()), 0x7F000001u);
    EXPECT_EQ((Ip{192, 168, 1, 7}.address()), 0xC0A80107u);
    EXPECT_EQ((Ip{}.address()), 0u);
}

TEST(Ip, LocalhostIsTheLoopbackAddress)
{
    EXPECT_EQ(Ip::localhost().address(), (Ip{127, 0, 0, 1}.address()));
    EXPECT_EQ(Ip::localhost().toStr(), "127.0.0.1");
}

TEST(Ip, ToStrRendersDottedDecimal)
{
    EXPECT_EQ((Ip{}.toStr()), "0.0.0.0");
    EXPECT_EQ((Ip{255, 255, 255, 255}.toStr()), "255.255.255.255");
    EXPECT_EQ((Ip{8, 8, 4, 4}.toStr()), "8.8.4.4");
}

TEST(VerifyPtr, AcceptsANonNullPointer)
{
    int value = 0;
    EXPECT_NO_THROW(verifyPtr(&value, "value", "test context"));
}

TEST(VerifyPtr, ThrowsOnANullPointerNamingTheContextAndTheElement)
{
    int *nothing = nullptr;
    try
    {
        verifyPtr(nothing, "widget", "creating pipeline");
        FAIL() << "a null pointer must throw";
    }
    catch (const std::runtime_error &error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("creating pipeline"), std::string::npos);
        EXPECT_NE(message.find("widget"), std::string::npos);
    }
}

} // namespace
