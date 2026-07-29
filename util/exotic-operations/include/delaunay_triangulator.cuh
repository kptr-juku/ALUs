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
#pragma once

#include <cstddef>

#include <cuda_runtime.h>

#include "delaunay_triangle2D.h"

namespace alus::delaunay {

inline size_t GetDelaunayTriangleCount(int width, int height) {
    if (width < 2 || height < 2) {
        return 0;
    }
    return static_cast<size_t>(width - 1) * static_cast<size_t>(height - 1) * 2;
}

cudaError_t LaunchDelaunayTriangulation(const double* x_coords, double x_multiplier, const double* y_coords,
                                        double y_multiplier, int width, int height, double invalid_index,
                                        DelaunayTriangle2D* triangles);

}  // namespace alus::delaunay
