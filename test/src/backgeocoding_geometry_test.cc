/** @file CPU-only regression tests for backgeocoding bounds and allocation sizes. */
#include "backgeocoding_geometry.h"

#include <limits>

#include <gtest/gtest.h>

namespace alus::backgeocoding {
namespace {

TEST(BackgeocodingGeometry, DisjointBoundsProduceEmptyRectangle) {
    // Actual Vivaldi IW2 failure: the mapped tile is beyond the secondary right edge.
    for (const auto rectangle : {RectangleFromInclusiveBounds(26754, 25588, 7540, 9047),
                                 RectangleFromInclusiveBounds(0, -2, 0, 10),
                                 RectangleFromInclusiveBounds(0, 10, 20, 10),
                                 RectangleFromInclusiveBounds(0, 10, 0, -2)}) {
        EXPECT_EQ(rectangle.width, 0);
        EXPECT_EQ(rectangle.height, 0);
        EXPECT_EQ(CheckedPixelCount(rectangle), 0);
    }
}

TEST(BackgeocodingGeometry, ValidBoundsPreserveOriginAndInclusiveSize) {
    const auto rectangle = RectangleFromInclusiveBounds(5, 9, 20, 22);
    EXPECT_EQ(rectangle.x, 5);
    EXPECT_EQ(rectangle.y, 20);
    EXPECT_EQ(rectangle.width, 5);
    EXPECT_EQ(rectangle.height, 3);
    EXPECT_EQ(CheckedPixelCount(rectangle), 15);
    EXPECT_EQ(CheckedPixelCount(RectangleFromInclusiveBounds(5, 5, 20, 20)), 1);
}

TEST(BackgeocodingGeometry, InvalidDimensionsAreRejected) {
    EXPECT_THROW(CheckedPixelCount(Rectangle{0, 0, -1165, 1508}), std::invalid_argument);
    EXPECT_THROW(CheckedPixelCount(Rectangle{0, 0, 1, -1}), std::invalid_argument);
    EXPECT_THROW(RectangleFromInclusiveBounds(0, std::numeric_limits<int>::max(), 0, 0), std::overflow_error);
    EXPECT_THROW(CheckedPixelCount(Rectangle{0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}),
                 std::overflow_error);
}

TEST(BackgeocodingGeometry, PixelCountDoesNotMultiplyAsSignedInt) {
    EXPECT_EQ(CheckedPixelCount(Rectangle{0, 0, 50000, 50000}), size_t{2500000000});
    EXPECT_EQ(CheckedPixelCount(Rectangle{0, 0, 0, 10}), 0);
}

}  // namespace
}  // namespace alus::backgeocoding
