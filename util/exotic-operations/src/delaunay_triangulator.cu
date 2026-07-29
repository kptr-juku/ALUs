/**
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see http://www.gnu.org/licenses/
 */
#include "delaunay_triangulator.cuh"

#include <limits>

#include "cuda_util.h"

namespace {

struct Point {
    double x;
    double y;
    int index;
    bool valid;
};

__device__ Point LoadPoint(const double* x_coords, double x_multiplier, const double* y_coords, double y_multiplier,
                           int index, double invalid_index) {
    const double x = x_coords[index];
    const double y = y_coords[index];
    return {x * x_multiplier, y * y_multiplier, index,
            x != invalid_index && y != invalid_index && isfinite(x) && isfinite(y)};
}

__device__ double Orient2d(const Point& a, const Point& b, const Point& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

__device__ double InCircle(const Point& a, const Point& b, const Point& c, const Point& d) {
    const double adx = a.x - d.x;
    const double ady = a.y - d.y;
    const double bdx = b.x - d.x;
    const double bdy = b.y - d.y;
    const double cdx = c.x - d.x;
    const double cdy = c.y - d.y;

    return (adx * adx + ady * ady) * (bdx * cdy - bdy * cdx) +
           (bdx * bdx + bdy * bdy) * (cdx * ady - cdy * adx) +
           (cdx * cdx + cdy * cdy) * (adx * bdy - ady * bdx);
}

__device__ void SetInvalid(alus::delaunay::DelaunayTriangle2D* triangle, double invalid_index) {
    triangle->ax = invalid_index;
    triangle->ay = invalid_index;
    triangle->bx = invalid_index;
    triangle->by = invalid_index;
    triangle->cx = invalid_index;
    triangle->cy = invalid_index;
    triangle->a_index = -1;
    triangle->b_index = -1;
    triangle->c_index = -1;
}

__device__ bool SetTriangle(const Point& a, Point b, Point c, alus::delaunay::DelaunayTriangle2D* triangle) {
    const double orientation = Orient2d(a, b, c);
    if (orientation == 0.0 || !isfinite(orientation)) {
        return false;
    }
    if (orientation < 0.0) {
        const Point temp = b;
        b = c;
        c = temp;
    }

    triangle->ax = a.x;
    triangle->ay = a.y;
    triangle->bx = b.x;
    triangle->by = b.y;
    triangle->cx = c.x;
    triangle->cy = c.y;
    triangle->a_index = a.index;
    triangle->b_index = b.index;
    triangle->c_index = c.index;
    return true;
}

__global__ void DelaunayTriangulation(const double* x_coords, double x_multiplier, const double* y_coords,
                                      double y_multiplier, int width, int height, double invalid_index,
                                      alus::delaunay::DelaunayTriangle2D* triangles) {
    const size_t cell_index = threadIdx.x + static_cast<size_t>(blockDim.x) * blockIdx.x;
    const size_t cell_count = static_cast<size_t>(width - 1) * (height - 1);
    if (cell_index >= cell_count) {
        return;
    }

    const int row = static_cast<int>(cell_index / (width - 1));
    const int column = static_cast<int>(cell_index % (width - 1));
    const int upper_left = row * width + column;
    const Point points[4] = {
        LoadPoint(x_coords, x_multiplier, y_coords, y_multiplier, upper_left, invalid_index),
        LoadPoint(x_coords, x_multiplier, y_coords, y_multiplier, upper_left + 1, invalid_index),
        LoadPoint(x_coords, x_multiplier, y_coords, y_multiplier, upper_left + width + 1, invalid_index),
        LoadPoint(x_coords, x_multiplier, y_coords, y_multiplier, upper_left + width, invalid_index),
    };

    auto* first_triangle = triangles + cell_index * 2;
    auto* second_triangle = first_triangle + 1;
    SetInvalid(first_triangle, invalid_index);
    SetInvalid(second_triangle, invalid_index);

    Point valid_points[4];
    int valid_count = 0;
    for (const Point& point : points) {
        if (point.valid) {
            valid_points[valid_count++] = point;
        }
    }

    if (valid_count == 3) {
        if (!SetTriangle(valid_points[0], valid_points[1], valid_points[2], first_triangle)) {
            SetInvalid(first_triangle, invalid_index);
        }
        return;
    }
    if (valid_count != 4) {
        return;
    }

    const double orientation_0 = Orient2d(points[0], points[1], points[2]);
    const double orientation_1 = Orient2d(points[1], points[2], points[3]);
    const double orientation_2 = Orient2d(points[2], points[3], points[0]);
    const double orientation_3 = Orient2d(points[3], points[0], points[1]);
    const bool convex_positive =
        orientation_0 > 0.0 && orientation_1 > 0.0 && orientation_2 > 0.0 && orientation_3 > 0.0;
    const bool convex_negative =
        orientation_0 < 0.0 && orientation_1 < 0.0 && orientation_2 < 0.0 && orientation_3 < 0.0;
    if (!convex_positive && !convex_negative) {
        return;
    }

    const bool use_other_diagonal = orientation_0 * InCircle(points[0], points[1], points[2], points[3]) > 0.0;
    bool first_valid;
    bool second_valid;
    if (use_other_diagonal) {
        first_valid = SetTriangle(points[0], points[1], points[3], first_triangle);
        second_valid = SetTriangle(points[1], points[2], points[3], second_triangle);
    } else {
        first_valid = SetTriangle(points[0], points[1], points[2], first_triangle);
        second_valid = SetTriangle(points[0], points[2], points[3], second_triangle);
    }
    if (!first_valid || !second_valid) {
        SetInvalid(first_triangle, invalid_index);
        SetInvalid(second_triangle, invalid_index);
    }
}

}  // namespace

namespace alus::delaunay {

cudaError_t LaunchDelaunayTriangulation(const double* x_coords, double x_multiplier, const double* y_coords,
                                        double y_multiplier, int width, int height, double invalid_index,
                                        DelaunayTriangle2D* triangles) {
    if (width < 2 || height < 2) {
        return cudaSuccess;
    }

    const size_t cell_count = static_cast<size_t>(width - 1) * static_cast<size_t>(height - 1);
    if (cell_count > std::numeric_limits<int>::max() || x_coords == nullptr || y_coords == nullptr ||
        triangles == nullptr) {
        return cudaErrorInvalidValue;
    }

    constexpr int block_size = 256;
    const int grid_size = cuda::GetGridDim(block_size, static_cast<int>(cell_count));
    DelaunayTriangulation<<<grid_size, block_size>>>(x_coords, x_multiplier, y_coords, y_multiplier, width, height,
                                                     invalid_index, triangles);
    return cudaGetLastError();
}

}  // namespace alus::delaunay
