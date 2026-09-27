
#include "fake_matrix.hpp"
#include "greedy_matrix.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using vcat::test::blankCell;
using vcat::test::cell;
using vcat::test::filledCell;
using vcat::test::flat_matrix;

namespace
{

/// Renders a matrix as one string per row, so a failed comparison shows the whole grid.
std::string render(const imatrix<char> &matrix)
{
    std::string rendered;
    for (int y = 0; y < matrix.height(); y++)
    {
        for (int x = 0; x < matrix.width(); x++)
        {
            rendered.push_back(matrix.get(x, y) == '\0' ? '.' : matrix.get(x, y));
        }
        rendered.push_back('\n');
    }
    return rendered;
}

/// Writes a value unique to each cell's coordinates, so a misplaced cell, a row overlapping its
/// neighbour or a byte left over from a previous size all show up.
void fillWithCoordinates(imatrix<int> &matrix)
{
    for (int y = 0; y < matrix.height(); y++)
    {
        for (int x = 0; x < matrix.width(); x++)
        {
            matrix.set(1000 + (y * 100) + x, x, y);
        }
    }
}

void expectCoordinates(const imatrix<int> &matrix)
{
    for (int y = 0; y < matrix.height(); y++)
    {
        for (int x = 0; x < matrix.width(); x++)
        {
            EXPECT_EQ(matrix.get(x, y), 1000 + (y * 100) + x) << "cell (" << x << "," << y << ")";
        }
    }
}

void expectAllZero(const imatrix<int> &matrix)
{
    for (int y = 0; y < matrix.height(); y++)
    {
        for (int x = 0; x < matrix.width(); x++)
        {
            EXPECT_EQ(matrix.get(x, y), 0) << "cell (" << x << "," << y << ")";
        }
    }
}

TEST(GreedyMatrix, TakesHeightFirstThenWidth)
{
    greedy_matrix<char> tall(7, 2);
    EXPECT_EQ(tall.height(), 7);
    EXPECT_EQ(tall.width(), 2);

    greedy_matrix<char> wide(2, 7);
    EXPECT_EQ(wide.height(), 2);
    EXPECT_EQ(wide.width(), 7);
}

TEST(GreedyMatrix, StartsFilledWithSpaces)
{
    greedy_matrix<char> matrix(3, 4);

    EXPECT_EQ(render(matrix), "    \n    \n    \n");
    EXPECT_EQ(matrix.get(0, 0), ' ');
    EXPECT_EQ(matrix.get(3, 2), ' ');
}

TEST(GreedyMatrix, FillsEveryByteOfACellWithASpace)
{
    // clear() memsets ' ' over the whole cell whatever T is, so a cell wider than a char is filled
    // the same way.
    greedy_matrix<int> matrix(2, 2);
    EXPECT_EQ(matrix.get(0, 0), blankCell<int>());

    greedy_matrix<cell> cells(2, 2);
    EXPECT_EQ(cells.get(1, 1), blankCell<cell>());
}

TEST(GreedyMatrix, SetAndGetRoundTrip)
{
    greedy_matrix<char> matrix(4, 6);
    matrix.set('a', 0, 0);
    matrix.set('b', 5, 0);
    matrix.set('c', 0, 3);
    matrix.set('d', 5, 3);
    matrix.set('e', 2, 1);

    EXPECT_EQ(render(matrix), "a    b\n  e   \n      \nc    d\n");
}

TEST(GreedyMatrix, XIsTheColumnAndYIsTheRow)
{
    // A transposed row/column mapping reads and writes different cells, so a value written on one
    // edge has to come back from the very same coordinates.
    greedy_matrix<char> matrix(2, 3);
    matrix.set('X', 2, 0);
    matrix.set('Y', 0, 1);

    EXPECT_EQ(matrix.get(2, 0), 'X');
    EXPECT_EQ(matrix.get(0, 1), 'Y');
    EXPECT_EQ(matrix.get(0, 0), ' ');
    EXPECT_EQ(matrix.get(1, 0), ' ');
    EXPECT_EQ(matrix.get(1, 1), ' ');
    EXPECT_EQ(matrix.get(2, 1), ' ');
}

TEST(GreedyMatrix, RowsDoNotOverlap)
{
    greedy_matrix<char> matrix(5, 4);
    for (int y = 0; y < matrix.height(); y++)
    {
        matrix.set(static_cast<char>('a' + y), 0, y);
    }

    EXPECT_EQ(render(matrix), "a   \nb   \nc   \nd   \ne   \n");
}

TEST(GreedyMatrix, EveryCellIsIndependent)
{
    greedy_matrix<int> matrix(3, 4);
    fillWithCoordinates(matrix);

    expectCoordinates(matrix);
}

TEST(GreedyMatrix, ReadsOutsideTheGridYieldADefaultConstructedValue)
{
    greedy_matrix<char> matrix(2, 3);
    matrix.set('k', 0, 0);

    EXPECT_EQ(matrix.get(0, 0), 'k') << "the in range cell must survive out of range reads";
    EXPECT_EQ(matrix.get(3, 0), '\0') << "x == width() is outside";
    EXPECT_EQ(matrix.get(0, 2), '\0') << "y == height() is outside";
    EXPECT_EQ(matrix.get(-1, 0), '\0') << "negative x is outside";
    EXPECT_EQ(matrix.get(0, -1), '\0') << "negative y is outside";

    greedy_matrix<int> numbers(2, 2);
    EXPECT_EQ(numbers.get(100, 100), 0);
}

TEST(GreedyMatrix, WritesOutsideTheGridAreDropped)
{
    greedy_matrix<char> matrix(2, 3);
    matrix.set('k', 0, 0);

    matrix.set('z', 3, 0);
    matrix.set('z', 0, 2);
    matrix.set('z', -1, 0);
    matrix.set('z', 0, -1);
    matrix.set('z', 12345, -6789);

    EXPECT_EQ(render(matrix), "k  \n   \n");
}

TEST(GreedyMatrix, IsUsableThroughAnImatrixReference)
{
    auto matrix = std::make_unique<greedy_matrix<char>>(2, 3);
    imatrix<char> &reference = *matrix;
    reference.set('v', 2, 1);

    EXPECT_EQ(reference.get(2, 1), 'v');
    EXPECT_EQ(reference.height(), 2);
    EXPECT_EQ(reference.width(), 3);
    EXPECT_NE(dynamic_cast<const greedy_matrix<char> *>(&reference), nullptr);
}

TEST(GreedyMatrix, ResizeChangesTheReportedDimensions)
{
    greedy_matrix<char> matrix(2, 2);

    matrix.resize(5, 9);
    EXPECT_EQ(matrix.height(), 5);
    EXPECT_EQ(matrix.width(), 9);

    matrix.resize(1, 1);
    EXPECT_EQ(matrix.height(), 1);
    EXPECT_EQ(matrix.width(), 1);

    matrix.resize(0, 4);
    EXPECT_EQ(matrix.height(), 0);
    EXPECT_EQ(matrix.width(), 4);
}

TEST(GreedyMatrix, ResizeZeroesTheCellsItLeavesVisible)
{
    // Unlike the constructor, allocateMat() memsets zeroes, so a resize hands back a zeroed grid
    // instead of the spaces a freshly built matrix has.
    greedy_matrix<char> matrix(3, 3);
    matrix.set('a', 0, 0);

    matrix.resize(4, 4);

    EXPECT_EQ(render(matrix), "....\n....\n....\n....\n");
}

TEST(GreedyMatrix, ResizeToTheSameDimensionsKeepsTheContents)
{
    greedy_matrix<char> matrix(3, 3);
    matrix.set('a', 0, 0);
    matrix.set('b', 2, 2);

    matrix.resize(3, 3);

    EXPECT_EQ(render(matrix), "a  \n   \n  b\n");
}

TEST(GreedyMatrix, GrowingKeepsEveryCellAddressable)
{
    greedy_matrix<int> matrix(2, 2);
    matrix.resize(6, 7);
    fillWithCoordinates(matrix);

    ASSERT_EQ(matrix.height(), 6);
    ASSERT_EQ(matrix.width(), 7);
    expectCoordinates(matrix);
}

TEST(GreedyMatrix, ShrinkingHidesTheCellsOutsideTheNewBounds)
{
    greedy_matrix<int> matrix(4, 4);
    fillWithCoordinates(matrix);

    matrix.resize(2, 3);

    ASSERT_EQ(matrix.height(), 2);
    ASSERT_EQ(matrix.width(), 3);
    for (int y = 0; y < matrix.height(); y++)
    {
        for (int x = 0; x < matrix.width(); x++)
        {
            EXPECT_EQ(matrix.get(x, y), 0) << "cell (" << x << "," << y << ")";
        }
    }
    EXPECT_EQ(matrix.get(3, 0), 0) << "the dropped column must read as empty";
    EXPECT_EQ(matrix.get(0, 3), 0) << "the dropped row must read as empty";
}

TEST(GreedyMatrix, GrowingBackAfterAShrinkDoesNotResurrectOldCells)
{
    greedy_matrix<int> matrix(4, 4);
    fillWithCoordinates(matrix);

    matrix.resize(2, 2);
    matrix.resize(4, 4);

    expectAllZero(matrix);
}

TEST(GreedyMatrix, SurvivesRepeatedResizesInBothDirections)
{
    greedy_matrix<int> matrix(1, 1);
    const std::vector<std::pair<int, int>> sizes{{1, 1},  {5, 2},  {2, 5}, {4, 4}, {8, 3}, {3, 8},
                                                 {1, 12}, {12, 1}, {6, 6}, {2, 2}, {5, 5}};

    for (const auto &[height, width] : sizes)
    {
        matrix.resize(height, width);
        ASSERT_EQ(matrix.height(), height) << "after resize to " << height << "x" << width;
        ASSERT_EQ(matrix.width(), width) << "after resize to " << height << "x" << width;

        fillWithCoordinates(matrix);
        expectCoordinates(matrix);
    }
}

TEST(GreedyMatrix, ResizeBetweenSizesThatNeedTheSameMemory)
{
    // 4x1 and 1x4 of int need the same number of bytes, so the second resize has to rebuild the row
    // pointers instead of reusing the layout of the first one.
    greedy_matrix<int> matrix(4, 1);

    matrix.resize(1, 4);
    fillWithCoordinates(matrix);
    ASSERT_EQ(matrix.height(), 1);
    ASSERT_EQ(matrix.width(), 4);
    for (int x = 0; x < 4; x++)
    {
        EXPECT_EQ(matrix.get(x, 0), 1000 + x) << "column " << x;
    }

    matrix.resize(4, 1);
    fillWithCoordinates(matrix);
    ASSERT_EQ(matrix.height(), 4);
    ASSERT_EQ(matrix.width(), 1);
    for (int y = 0; y < 4; y++)
    {
        EXPECT_EQ(matrix.get(0, y), 1000 + (y * 100)) << "row " << y;
    }
}

TEST(GreedyMatrix, CopyConstructionCopiesContentsAndDimensions)
{
    greedy_matrix<char> source(3, 4);
    source.set('a', 0, 0);
    source.set('b', 3, 2);

    greedy_matrix<char> copy(source);

    EXPECT_EQ(copy.height(), 3);
    EXPECT_EQ(copy.width(), 4);
    EXPECT_EQ(render(copy), render(source));
}

TEST(GreedyMatrix, CopyConstructionDoesNotAliasTheSource)
{
    greedy_matrix<char> source(2, 3);
    source.set('a', 0, 0);

    greedy_matrix<char> copy(source);
    copy.set('z', 2, 1);

    EXPECT_EQ(source.get(2, 1), ' ') << "writing to the copy must not reach the source";
    EXPECT_EQ(copy.get(0, 0), 'a');

    source.set('q', 1, 1);
    EXPECT_EQ(copy.get(1, 1), ' ') << "writing to the source must not reach the copy";
    EXPECT_EQ(copy.get(0, 0), 'a');
}

TEST(GreedyMatrix, CopyAssignmentCopiesContentsAndDimensions)
{
    greedy_matrix<char> source(3, 4);
    source.set('a', 0, 0);
    source.set('b', 3, 2);

    greedy_matrix<char> target(1, 1);
    target.set('x', 0, 0);

    target = source;

    EXPECT_EQ(target.height(), 3);
    EXPECT_EQ(target.width(), 4);
    EXPECT_EQ(render(target), render(source));
    EXPECT_EQ(render(source), "a   \n    \n   b\n");
}

TEST(GreedyMatrix, CopyAssignmentOverALargerMatrixDropsTheExtraCells)
{
    greedy_matrix<char> source(2, 2);
    source.set('a', 0, 0);
    source.set('b', 1, 1);

    greedy_matrix<char> target(4, 4);
    target.set('x', 3, 3);

    target = source;

    EXPECT_EQ(target.height(), 2);
    EXPECT_EQ(target.width(), 2);
    EXPECT_EQ(render(target), "a \n b\n");
    EXPECT_EQ(target.get(3, 3), '\0') << "cells outside the new bounds must read as empty";
}

TEST(GreedyMatrix, SelfAssignmentKeepsTheContents)
{
    greedy_matrix<char> matrix(2, 3);
    matrix.set('a', 0, 0);
    matrix.set('b', 2, 1);

    greedy_matrix<char> &alias = matrix;
    matrix = alias;

    EXPECT_EQ(matrix.height(), 2);
    EXPECT_EQ(matrix.width(), 3);
    EXPECT_EQ(render(matrix), "a  \n  b\n");
}

TEST(GreedyMatrix, CopyFromFillsTheMatrixThroughAnImatrixReference)
{
    greedy_matrix<char> source(2, 3);
    source.set('a', 0, 0);
    source.set('b', 1, 1);
    source.set('c', 2, 1);

    greedy_matrix<char> destination(5, 5);
    destination.set('x', 4, 4);
    imatrix<char> &destinationRef = destination;

    destinationRef.copy_from(source);

    EXPECT_EQ(destination.height(), 2);
    EXPECT_EQ(destination.width(), 3);
    EXPECT_EQ(render(destination), "a  \n bc\n");
}

TEST(GreedyMatrix, CopyFromBetweenDifferentSizesCopiesEveryVisibleCell)
{
    greedy_matrix<int> source(2, 5);
    fillWithCoordinates(source);

    greedy_matrix<int> destination(7, 1);
    imatrix<int> &destinationRef = destination;

    destinationRef.copy_from(source);

    ASSERT_EQ(destination.height(), 2);
    ASSERT_EQ(destination.width(), 5);
    expectCoordinates(destination);
    EXPECT_EQ(source.height(), 2) << "the source must survive the copy";
    EXPECT_EQ(source.width(), 5);
    EXPECT_EQ(source.get(4, 1), 1104);
}

TEST(GreedyMatrix, CopyFromItselfIsSafe)
{
    greedy_matrix<char> matrix(3, 3);
    matrix.set('a', 1, 1);
    imatrix<char> &matrixRef = matrix;

    matrixRef.copy_from(matrixRef);

    EXPECT_EQ(matrix.height(), 3);
    EXPECT_EQ(matrix.width(), 3);
    EXPECT_EQ(render(matrix), "   \n a \n   \n");
}

TEST(GreedyMatrix, CopyFromRejectsASourceThatIsNotAGreedyMatrix)
{
    flat_matrix<char> source(2, 2);
    source.set('a', 0, 0);
    greedy_matrix<char> destination(3, 3);
    destination.set('k', 0, 0);
    imatrix<char> &destinationRef = destination;
    const imatrix<char> &sourceRef = source;

    EXPECT_THROW(destinationRef.copy_from(sourceRef), std::invalid_argument);

    EXPECT_EQ(destination.height(), 3) << "a rejected copy must not resize the destination";
    EXPECT_EQ(destination.width(), 3);
    EXPECT_EQ(render(destination), "k  \n   \n   \n");
}

TEST(GreedyMatrix, HandlesDegenerateSizes)
{
    greedy_matrix<char> empty(0, 0);
    EXPECT_EQ(empty.height(), 0);
    EXPECT_EQ(empty.width(), 0);
    EXPECT_EQ(empty.get(0, 0), '\0');
    empty.set('z', 0, 0);
    EXPECT_EQ(render(empty), "");

    greedy_matrix<char> noRows(0, 4);
    EXPECT_EQ(noRows.height(), 0);
    EXPECT_EQ(noRows.width(), 4);
    EXPECT_EQ(noRows.get(0, 0), '\0');
    noRows.set('z', 0, 0);
    EXPECT_EQ(render(noRows), "");

    greedy_matrix<char> noColumns(4, 0);
    EXPECT_EQ(noColumns.height(), 4);
    EXPECT_EQ(noColumns.width(), 0);
    EXPECT_EQ(noColumns.get(0, 0), '\0');
    noColumns.set('z', 0, 0);
    EXPECT_EQ(render(noColumns), "\n\n\n\n");

    greedy_matrix<char> single(1, 1);
    single.set('*', 0, 0);
    EXPECT_EQ(render(single), "*\n");
}

TEST(GreedyMatrix, RecoversFromADegenerateSize)
{
    greedy_matrix<int> matrix(4, 0);

    matrix.resize(2, 2);

    ASSERT_EQ(matrix.height(), 2);
    ASSERT_EQ(matrix.width(), 2);
    fillWithCoordinates(matrix);
    expectCoordinates(matrix);
}

using GreedyMatrixTypes = ::testing::Types<char, int, unsigned int, uint32_t, cell>;
template <typename T> struct GreedyMatrixValueTypes : public ::testing::Test
{
};
TYPED_TEST_SUITE(GreedyMatrixValueTypes, GreedyMatrixTypes);

TYPED_TEST(GreedyMatrixValueTypes, SetAndGetRoundTrip)
{
    greedy_matrix<TypeParam> matrix(3, 4);
    const TypeParam written = filledCell<TypeParam>('Z');
    ASSERT_NE(written, blankCell<TypeParam>()) << "the written cell must differ from the initial fill";

    matrix.set(written, 0, 0);
    matrix.set(written, 3, 2);

    EXPECT_EQ(matrix.get(0, 0), written);
    EXPECT_EQ(matrix.get(3, 2), written);
    EXPECT_EQ(matrix.get(1, 1), blankCell<TypeParam>());
}

TYPED_TEST(GreedyMatrixValueTypes, CopyingAndResizingKeepTheTwoMatricesApart)
{
    greedy_matrix<TypeParam> source(2, 3);
    const TypeParam written = filledCell<TypeParam>('Z');
    source.set(written, 1, 1);
    source.set(blankCell<TypeParam>(), 0, 0);

    greedy_matrix<TypeParam> copy(source);
    EXPECT_EQ(copy.get(1, 1), written) << "the copy must hold what the source held";

    copy.resize(4, 4);
    copy.set(written, 3, 3);

    EXPECT_EQ(copy.get(3, 3), written);
    EXPECT_EQ(copy.height(), 4);
    EXPECT_EQ(copy.width(), 4);
    EXPECT_EQ(copy.get(1, 1), TypeParam{}) << "a resize hands back a fresh grid, so the old cell is gone";
    EXPECT_EQ(source.get(1, 1), written) << "resizing the copy must not reach the source";
    EXPECT_EQ(source.height(), 2);
    EXPECT_EQ(source.width(), 3);
}

TYPED_TEST(GreedyMatrixValueTypes, OutOfBoundsAccessIsHarmless)
{
    greedy_matrix<TypeParam> matrix(2, 2);

    matrix.set(filledCell<TypeParam>('Z'), 5, 5);

    EXPECT_EQ(matrix.get(5, 5), TypeParam{});
    EXPECT_EQ(matrix.get(-5, -5), TypeParam{});
    EXPECT_EQ(matrix.get(0, 0), blankCell<TypeParam>());
}

} // namespace
