/** @file Host-side validation of backgeocoding tile geometry. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "shapes.h"

namespace alus::backgeocoding {

/** Convert clipped inclusive bounds to a rectangle; disjoint bounds mean no coverage. */
inline Rectangle RectangleFromInclusiveBounds(int x_min, int x_max, int y_min, int y_max) {
    if (x_min > x_max || y_min > y_max) {
        return {0, 0, 0, 0};
    }
    const auto width = static_cast<int64_t>(x_max) - x_min + 1;
    const auto height = static_cast<int64_t>(y_max) - y_min + 1;
    if (width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max()) {
        throw std::overflow_error("Backgeocoding rectangle dimensions exceed int range");
    }
    return {x_min, y_min, static_cast<int>(width), static_cast<int>(height)};
}

/** Validate signed dimensions before converting and multiplying allocation sizes. */
inline size_t CheckedPixelCount(const Rectangle& rectangle) {
    if (rectangle.width < 0 || rectangle.height < 0) {
        throw std::invalid_argument("Negative backgeocoding rectangle dimensions");
    }
    if (rectangle.width == 0 || rectangle.height == 0) {
        return 0;
    }
    const auto width = static_cast<size_t>(rectangle.width);
    const auto height = static_cast<size_t>(rectangle.height);
    // Backgeocoding allocates double arrays as well as int16/float arrays per pixel.
    if (width > std::numeric_limits<size_t>::max() / sizeof(double) / height) {
        throw std::overflow_error("Backgeocoding tile allocation size overflow");
    }
    return width * height;
}

}  // namespace alus::backgeocoding
