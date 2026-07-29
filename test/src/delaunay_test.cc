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
#include <fstream>
#include <vector>

#include "gmock/gmock.h"

#include "backgeocoding_constants.h"
#include "comparators.h"
#include "cuda_util.h"
#include "delaunay_triangle2D.h"
#include "delaunay_triangulator.h"
#include "delaunay_triangulator.cuh"

namespace {

class DelaunayTriangulationTester {
public:
    std::vector<double> x_coords_;
    std::vector<double> y_coords_;
    std::vector<alus::delaunay::DelaunayTriangle2D> triangles_;
    int width_, height_, triangle_size_;
    double* device_x_coords_ = nullptr;
    double* device_y_coords_ = nullptr;
    alus::delaunay::DelaunayTriangle2D* device_triangles_ = nullptr;
    const double invalid_index = alus::backgeocoding::INVALID_INDEX;

    DelaunayTriangulationTester() = default;

    void ReadSmallTestData() {
        alus::delaunay::DelaunayTriangle2D temp_triangle;
        this->width_ = 6;  // NOLINT
        this->height_ = 1;
        this->triangle_size_ = 5;  // maximum triangles include 2n -2 -b   // NOLINT
        x_coords_.resize(this->width_);
        y_coords_.resize(this->width_);

        x_coords_.at(0) = 1;
        y_coords_.at(0) = 1;

        x_coords_.at(1) = 2;
        y_coords_.at(1) = 2.5;  // NOLINT

        x_coords_.at(2) = 3;  // NOLINT
        y_coords_.at(2) = 1;

        x_coords_.at(3) = 4;    // NOLINT
        y_coords_.at(3) = 2.5;  // NOLINT

        x_coords_.at(4) = 3.5;  // NOLINT
        y_coords_.at(4) = 4;    // NOLINT

        x_coords_.at(5) = 2.5;  // NOLINT
        y_coords_.at(5) = 4;    // NOLINT

        this->triangles_.resize(this->triangle_size_);

        // triangle 1
        temp_triangle.ax = 1;
        temp_triangle.ay = 1;
        temp_triangle.a_index = 0;

        temp_triangle.bx = 2;
        temp_triangle.by = 2.5;  // NOLINT
        temp_triangle.b_index = 1;

        temp_triangle.cx = 3;  // NOLINT
        temp_triangle.cy = 1;
        temp_triangle.c_index = 2;
        this->triangles_.at(0) = temp_triangle;

        // triangle 2
        temp_triangle.ax = 1;
        temp_triangle.ay = 1;
        temp_triangle.a_index = 0;

        temp_triangle.bx = 2;
        temp_triangle.by = 2.5;  // NOLINT
        temp_triangle.b_index = 1;

        temp_triangle.cx = 2.5;     // NOLINT
        temp_triangle.cy = 4;       // NOLINT
        temp_triangle.c_index = 5;  // NOLINT
        this->triangles_.at(1) = temp_triangle;

        // triangle 3
        temp_triangle.ax = 4;       // NOLINT
        temp_triangle.ay = 2.5;     // NOLINT
        temp_triangle.a_index = 3;  // NOLINT

        temp_triangle.bx = 2;
        temp_triangle.by = 2.5;  // NOLINT
        temp_triangle.b_index = 1;

        temp_triangle.cx = 3;  // NOLINT
        temp_triangle.cy = 1;
        temp_triangle.c_index = 2;
        this->triangles_.at(2) = temp_triangle;

        // triangle 4
        temp_triangle.ax = 4;       // NOLINT
        temp_triangle.ay = 2.5;     // NOLINT
        temp_triangle.a_index = 3;  // NOLINT

        temp_triangle.bx = 3.5;     // NOLINT
        temp_triangle.by = 4;       // NOLINT
        temp_triangle.b_index = 4;  // NOLINT

        temp_triangle.cx = 2.5;                  // NOLINT
        temp_triangle.cy = 4;                    // NOLINT
        temp_triangle.c_index = 5;               // NOLINT
        this->triangles_.at(3) = temp_triangle;  // NOLINT

        // triangle 5
        temp_triangle.bx = 2;
        temp_triangle.by = 2.5;  // NOLINT
        temp_triangle.b_index = 1;

        temp_triangle.ax = 4;       // NOLINT
        temp_triangle.ay = 2.5;     // NOLINT
        temp_triangle.a_index = 3;  // NOLINT

        temp_triangle.cx = 2.5;                  // NOLINT
        temp_triangle.cy = 4;                    // NOLINT
        temp_triangle.c_index = 5;               // NOLINT
        this->triangles_.at(4) = temp_triangle;  // NOLINT
    }
};

std::vector<alus::delaunay::DelaunayTriangle2D> TriangulateGridOnGpu(const std::vector<double>& x_coords,
                                                                     const std::vector<double>& y_coords, int width,
                                                                     int height, double invalid_index) {
    double* device_x_coords = nullptr;
    double* device_y_coords = nullptr;
    alus::delaunay::DelaunayTriangle2D* device_triangles = nullptr;
    const size_t point_count = static_cast<size_t>(width) * height;
    const size_t triangle_count = alus::delaunay::GetDelaunayTriangleCount(width, height);

    CHECK_CUDA_ERR(cudaMalloc(&device_x_coords, point_count * sizeof(double)));
    CHECK_CUDA_ERR(cudaMalloc(&device_y_coords, point_count * sizeof(double)));
    CHECK_CUDA_ERR(cudaMalloc(&device_triangles, triangle_count * sizeof(alus::delaunay::DelaunayTriangle2D)));
    CHECK_CUDA_ERR(
        cudaMemcpy(device_x_coords, x_coords.data(), point_count * sizeof(double), cudaMemcpyHostToDevice));
    CHECK_CUDA_ERR(
        cudaMemcpy(device_y_coords, y_coords.data(), point_count * sizeof(double), cudaMemcpyHostToDevice));

    CHECK_CUDA_ERR(alus::delaunay::LaunchDelaunayTriangulation(
        device_x_coords, 1.0, device_y_coords, 1.0, width, height, invalid_index, device_triangles));

    std::vector<alus::delaunay::DelaunayTriangle2D> triangles(triangle_count);
    CHECK_CUDA_ERR(cudaMemcpy(triangles.data(), device_triangles,
                              triangle_count * sizeof(alus::delaunay::DelaunayTriangle2D), cudaMemcpyDeviceToHost));
    CHECK_CUDA_ERR(cudaFree(device_triangles));
    CHECK_CUDA_ERR(cudaFree(device_y_coords));
    CHECK_CUDA_ERR(cudaFree(device_x_coords));
    return triangles;
}

double Orientation(const alus::delaunay::DelaunayTriangle2D& triangle) {
    return (triangle.bx - triangle.ax) * (triangle.cy - triangle.ay) -
           (triangle.by - triangle.ay) * (triangle.cx - triangle.ax);
}

// don't run this unless you are developing the delaunay gpu algorithm or it has been finished.
/*TEST(DelaunayTest, TriangulationTest){
    alus::delaunay::DelaunayTriangle2D temp_triangle;
    DelaunayTriangulationTester tester;
    tester.readTestData();
    tester.HostToDevice();

    CHECK_CUDA_ERR(alus::delaunay::LaunchDelaunayTriangulation(tester.device_x_coords_, tester.device_y_coords_,
tester.width_, tester.height_, tester.device_triangles_));

    tester.DeviceToHost();

    for(int i=0; i<tester.triangle_size_; i++){
        temp_triangle = tester.triangles_.at(i);
        std::cout<<i<<") "<< temp_triangle.ax << " " << temp_triangle.ay << " " ;
        std::cout<<  temp_triangle.bx << " " << temp_triangle.by << " ";
        std::cout<<  temp_triangle.cx << " " << temp_triangle.cy;
        std::cout<< " previous: " << temp_triangle.previous<< std::endl;
    }

}*/

TEST(DelaunayTest, SmallCPUTriangulationTest) {
    DelaunayTriangulationTester tester;
    tester.ReadSmallTestData();

    alus::delaunay::DelaunayTriangulator trianglulator;
    trianglulator.TriangulateCPU(tester.x_coords_.data(), 1.0, tester.y_coords_.data(), 1.0, tester.width_,
                                 tester.invalid_index);

    size_t count = alus::EqualsTriangles(trianglulator.host_triangles_.data(), tester.triangles_.data(),
                                         trianglulator.triangle_count_, 0.00001);  // NOLINT
    EXPECT_EQ(count, 0) << "Triangle results do not match. Mismatches: " << count << '\n';
}

TEST(DelaunayTest, CudaGridTriangulationUsesDeterministicDelaunayDiagonal) {
    constexpr int width = 2;
    constexpr int height = 2;
    constexpr double invalid_index = alus::backgeocoding::INVALID_INDEX;
    const std::vector<double> x_coords{0.0, 0.0, 1.0, 1.0};
    const std::vector<double> y_coords{0.0, 1.0, 0.0, 1.0};

    const auto triangles = TriangulateGridOnGpu(x_coords, y_coords, width, height, invalid_index);

    ASSERT_EQ(triangles.size(), 2);
    EXPECT_GT(Orientation(triangles[0]), 0.0);
    EXPECT_GT(Orientation(triangles[1]), 0.0);
    const std::vector<int> first_indices{triangles[0].a_index, triangles[0].b_index, triangles[0].c_index};
    const std::vector<int> second_indices{triangles[1].a_index, triangles[1].b_index, triangles[1].c_index};
    EXPECT_THAT(first_indices, ::testing::UnorderedElementsAre(0, 1, 3));
    EXPECT_THAT(second_indices, ::testing::UnorderedElementsAre(0, 2, 3));
}

TEST(DelaunayTest, CudaGridTriangulationFlipsNonDelaunayDiagonal) {
    constexpr int width = 2;
    constexpr int height = 2;
    constexpr double invalid_index = alus::backgeocoding::INVALID_INDEX;
    const std::vector<double> x_coords{0.0, 0.0, 1.0, 2.0};
    const std::vector<double> y_coords{0.0, 2.0, 0.0, 2.0};

    const auto triangles = TriangulateGridOnGpu(x_coords, y_coords, width, height, invalid_index);

    ASSERT_EQ(triangles.size(), 2);
    const std::vector<int> first_indices{triangles[0].a_index, triangles[0].b_index, triangles[0].c_index};
    const std::vector<int> second_indices{triangles[1].a_index, triangles[1].b_index, triangles[1].c_index};
    EXPECT_THAT(first_indices, ::testing::UnorderedElementsAre(0, 1, 2));
    EXPECT_THAT(second_indices, ::testing::UnorderedElementsAre(1, 2, 3));
}

TEST(DelaunayTest, CudaGridTriangulationKeepsInvalidPointAsHole) {
    constexpr int width = 2;
    constexpr int height = 2;
    constexpr double invalid_index = alus::backgeocoding::INVALID_INDEX;
    const std::vector<double> x_coords{0.0, 0.0, 1.0, invalid_index};
    const std::vector<double> y_coords{0.0, 1.0, 0.0, invalid_index};

    const auto triangles = TriangulateGridOnGpu(x_coords, y_coords, width, height, invalid_index);

    ASSERT_EQ(triangles.size(), 2);
    EXPECT_GT(Orientation(triangles[0]), 0.0);
    const std::vector<int> valid_indices{triangles[0].a_index, triangles[0].b_index, triangles[0].c_index};
    EXPECT_THAT(valid_indices, ::testing::UnorderedElementsAre(0, 1, 2));
    EXPECT_EQ(triangles[1].ax, invalid_index);
    EXPECT_EQ(triangles[1].a_index, -1);
}

// NOLINTNEXTLINE
// TODO: We will bring this back once we have the full algorithm, so we can test speeds and accuracies of end results.
/*TEST(DelaunayTest, BigCPUTriangulationTest2){
    DelaunayTriangulationTester tester;
    tester.readBigTestData();

    alus::delaunay::DelaunayTriangulator trianglulator;
    trianglulator.TriangulateCPU(tester.x_coords_.data(), tester.y_coords_.data(), tester.width_ * tester.height_);
    std::cout <<"nr of triangles: " << trianglulator.triangle_count_ <<std::endl;

    int count = alus::EqualsTriangles(trianglulator.host_triangles_.data(), tester.triangles_.data(),
trianglulator.triangle_count_, 0.00001); EXPECT_EQ(count,0) << "Triangle results do not match. Mismatches: " << count <<
'\n';

}*/

}  // namespace
