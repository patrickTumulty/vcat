
#include "fake_matrix.hpp"
#include "imatrix.hpp"
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

using vcat::test::cell;
using vcat::test::filledCell;
using vcat::test::flat_matrix;
using vcat::test::recording_matrix;

namespace
{

/// The interface is a pure contract: a caller can only hold an imatrix through one of these
/// operations, so every one of them has to be reachable and consistent through a base reference.
TEST(Imatrix, IsAbstract)
{
    static_assert(std::is_abstract_v<imatrix<int>>);
    static_assert(!std::is_abstract_v<flat_matrix<int>>);

    EXPECT_TRUE((std::is_abstract_v<imatrix<char>>));
    EXPECT_TRUE((std::is_abstract_v<imatrix<cell>>));
}

using ImatrixTypes = ::testing::Types<char, int, unsigned int, uint32_t, cell>;
template <typename T> struct ImatrixContract : public ::testing::Test
{
};
TYPED_TEST_SUITE(ImatrixContract, ImatrixTypes);

TYPED_TEST(ImatrixContract, ReportsTheDimensionsItWasBuiltWith)
{
    flat_matrix<TypeParam> implementation(3, 7);
    imatrix<TypeParam> &matrix = implementation;

    EXPECT_EQ(matrix.height(), 3);
    EXPECT_EQ(matrix.width(), 7);
}

TYPED_TEST(ImatrixContract, SetThenGetRoundTripsThroughTheBaseReference)
{
    flat_matrix<TypeParam> implementation(4, 5);
    imatrix<TypeParam> &matrix = implementation;
    const TypeParam written = filledCell<TypeParam>('K');

    matrix.set(written, 2, 1);

    EXPECT_EQ(matrix.get(2, 1), written);
}

TYPED_TEST(ImatrixContract, KeepsEveryCellIndependent)
{
    flat_matrix<TypeParam> implementation(3, 4);
    imatrix<TypeParam> &matrix = implementation;
    matrix.set(filledCell<TypeParam>('K'), 3, 2);

    for (int y = 0; y < matrix.height(); y++)
    {
        for (int x = 0; x < matrix.width(); x++)
        {
            if (x == 3 && y == 2)
            {
                continue;
            }
            EXPECT_EQ(matrix.get(x, y), TypeParam{}) << "cell (" << x << "," << y << ") was overwritten";
        }
    }
}

TYPED_TEST(ImatrixContract, ReadsOutsideTheGridYieldADefaultConstructedValue)
{
    flat_matrix<TypeParam> implementation(2, 3);
    imatrix<TypeParam> &matrix = implementation;

    EXPECT_EQ(matrix.get(0, 0), TypeParam{});
    EXPECT_EQ(matrix.get(3, 0), TypeParam{}) << "x == width() is outside";
    EXPECT_EQ(matrix.get(0, 2), TypeParam{}) << "y == height() is outside";
    EXPECT_EQ(matrix.get(-1, 0), TypeParam{}) << "negative x is outside";
    EXPECT_EQ(matrix.get(0, -1), TypeParam{}) << "negative y is outside";
}

TYPED_TEST(ImatrixContract, WritesOutsideTheGridAreDropped)
{
    flat_matrix<TypeParam> implementation(2, 3);
    imatrix<TypeParam> &matrix = implementation;
    matrix.set(filledCell<TypeParam>('K'), 0, 0);

    matrix.set(filledCell<TypeParam>('Z'), 3, 0);
    matrix.set(filledCell<TypeParam>('Z'), 0, 2);
    matrix.set(filledCell<TypeParam>('Z'), -1, 0);
    matrix.set(filledCell<TypeParam>('Z'), 0, -1);
    matrix.set(filledCell<TypeParam>('Z'), 12345, -6789);

    EXPECT_EQ(matrix.get(0, 0), filledCell<TypeParam>('K')) << "a dropped write must not touch in range cells";
}

TYPED_TEST(ImatrixContract, AnEmptyMatrixIsUsable)
{
    flat_matrix<TypeParam> implementation(0, 0);
    imatrix<TypeParam> &matrix = implementation;

    EXPECT_EQ(matrix.height(), 0);
    EXPECT_EQ(matrix.width(), 0);
    EXPECT_EQ(matrix.get(0, 0), TypeParam{});

    matrix.set(filledCell<TypeParam>('Z'), 0, 0);
    EXPECT_EQ(matrix.get(0, 0), TypeParam{});
}

/// The interface says nothing about the cell type, so an implementation may hold values that own
/// their storage, not just plain numbers.
TEST(Imatrix, CarriesValuesThatOwnTheirStorage)
{
    flat_matrix<std::string> implementation(2, 3);
    imatrix<std::string> &matrix = implementation;
    matrix.set("ascii", 2, 1);

    EXPECT_EQ(matrix.get(2, 1), "ascii");
    EXPECT_EQ(matrix.get(0, 0), "");

    flat_matrix<std::string> other(1, 1);
    other.set("copied", 0, 0);
    matrix.copy_from(other);

    EXPECT_EQ(matrix.get(0, 0), "copied");
    EXPECT_EQ(matrix.height(), 1);
    EXPECT_EQ(matrix.width(), 1);
}

TEST(Imatrix, ResizeIsVisibleThroughTheSameBaseReference)
{
    flat_matrix<char> implementation(2, 2);
    imatrix<char> &matrix = implementation;
    matrix.set('a', 0, 0);

    matrix.resize(5, 9);

    EXPECT_EQ(matrix.height(), 5);
    EXPECT_EQ(matrix.width(), 9);
    matrix.set('a', 0, 0);
    EXPECT_EQ(matrix.get(0, 0), 'a');

    matrix.resize(1, 1);
    EXPECT_EQ(matrix.height(), 1);
    EXPECT_EQ(matrix.width(), 1);
    matrix.set('b', 0, 0);
    EXPECT_EQ(matrix.get(0, 0), 'b');
}

TEST(Imatrix, EveryOperationReachesTheImplementation)
{
    recording_matrix<char> recorder(3, 4);
    imatrix<char> &matrix = recorder;
    recorder.clearCalls();

    EXPECT_EQ(matrix.height(), 3);
    EXPECT_EQ(matrix.width(), 4);
    matrix.set('q', 1, 2);
    EXPECT_EQ(matrix.get(1, 2), 'q');
    matrix.resize(6, 7);
    EXPECT_EQ(matrix.height(), 6);
    EXPECT_EQ(matrix.width(), 7);

    const std::vector<std::string> expected{"height()",    "width()",  "set(1,2)", "get(1,2)",
                                            "resize(6,7)", "height()", "width()"};
    EXPECT_EQ(recorder.calls, expected);
}

TEST(Imatrix, CopyFromReachesTheImplementationWithTheSourceDimensions)
{
    recording_matrix<char> recorder(3, 4);
    imatrix<char> &destination = recorder;
    recorder.clearCalls();
    flat_matrix<char> source(5, 6);

    destination.copy_from(source);

    const std::vector<std::string> expected{"copy_from(5,6)"};
    EXPECT_EQ(recorder.calls, expected);
}

TEST(Imatrix, CopyFromFillsTheDestinationFromTheSource)
{
    flat_matrix<char> source(2, 3);
    source.set('a', 0, 0);
    source.set('b', 2, 0);
    source.set('c', 1, 1);

    flat_matrix<char> destination(9, 9);
    destination.set('d', 8, 8);
    imatrix<char> &destinationRef = destination;

    destinationRef.copy_from(source);

    ASSERT_EQ(destinationRef.height(), 2) << "the destination must adopt the source dimensions";
    ASSERT_EQ(destinationRef.width(), 3);
    EXPECT_EQ(destinationRef.get(0, 0), 'a');
    EXPECT_EQ(destinationRef.get(2, 0), 'b');
    EXPECT_EQ(destinationRef.get(1, 1), 'c');

    EXPECT_EQ(source.get(0, 0), 'a') << "the source must be left alone";
    EXPECT_EQ(source.height(), 2);
    EXPECT_EQ(source.width(), 3);
}

TEST(Imatrix, CopyFromWorksBetweenDifferentImplementations)
{
    recording_matrix<int> source(2, 2);
    source.value = 42;
    imatrix<int> &sourceRef = source;

    flat_matrix<int> destination(1, 1);
    imatrix<int> &destinationRef = destination;

    destinationRef.copy_from(sourceRef);

    EXPECT_EQ(destinationRef.height(), 2);
    EXPECT_EQ(destinationRef.width(), 2);
    EXPECT_EQ(destinationRef.get(0, 0), 42);
    EXPECT_EQ(destinationRef.get(1, 1), 42);
}

TEST(Imatrix, CopyFromItselfIsSafe)
{
    flat_matrix<char> matrix(3, 3);
    matrix.set('k', 1, 1);
    imatrix<char> &matrixRef = matrix;

    matrixRef.copy_from(matrixRef);

    EXPECT_EQ(matrix.height(), 3);
    EXPECT_EQ(matrix.width(), 3);
    EXPECT_EQ(matrix.get(1, 1), 'k');
}

TEST(Imatrix, TheBaseTypeDoesNotFavourOneImplementation)
{
    // imatrix has no virtual destructor, so deleting through a base pointer would skip the derived
    // one. Each implementation is owned by a pointer to its own type and only borrowed as an
    // imatrix, which is the safe way to hold one of these behind the interface.
    std::unique_ptr<flat_matrix<char>> flat = std::make_unique<flat_matrix<char>>(2, 2);
    std::unique_ptr<recording_matrix<char>> recorder = std::make_unique<recording_matrix<char>>(2, 2);
    imatrix<char> *implementations[] = {flat.get(), recorder.get()};

    for (auto *implementation : implementations)
    {
        implementation->set('x', 0, 0);
        EXPECT_EQ(implementation->get(0, 0), 'x');
        implementation->resize(3, 3);
        EXPECT_EQ(implementation->height(), 3);
    }

    EXPECT_NE(dynamic_cast<flat_matrix<char> *>(implementations[0]), nullptr);
    EXPECT_EQ(dynamic_cast<recording_matrix<char> *>(implementations[0]), nullptr);
    EXPECT_EQ(dynamic_cast<flat_matrix<char> *>(implementations[1]), nullptr);
    EXPECT_NE(dynamic_cast<recording_matrix<char> *>(implementations[1]), nullptr);

    // A shared_ptr built from the concrete type also remembers it, so a base pointer can own one.
    std::shared_ptr<imatrix<char>> owned = std::make_shared<flat_matrix<char>>(2, 2);
    owned->set('y', 1, 1);
    EXPECT_EQ(owned->get(1, 1), 'y');
    EXPECT_EQ(owned->width(), 2);
}

} // namespace
