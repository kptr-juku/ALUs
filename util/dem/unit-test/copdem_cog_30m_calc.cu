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

#include <array>
#include <cmath>

#include "gmock/gmock.h"

#include "copdem_cog_30m_calc.cuh"
#include "cuda_ptr.h"

namespace {

struct SamplesResult {
    int all_valid;
    double samples[4];  // NOLINT
};

__global__ void GetSamplesKernel(alus::PointerArray tiles, alus::dem::Property property, int x0, int x1,
                                 SamplesResult* result) {
    int x[2]{x0, x1};
    int y[2]{0, 1};
    result->all_valid = alus::dem::GetSamples(&tiles, x, y, result->samples, 2, 2, property.no_data_value, 1,
                                              &property);
}

SamplesResult GetSamples(int x0, int x1, const std::array<float, 4>& values) {
    alus::dem::Property property{};
    property.tile_pixel_count_inverted_x = 0.5;
    property.tile_pixel_count_inverted_y = 0.5;
    property.tile_pixel_count_x = 2;
    property.tile_pixel_count_y = 2;
    property.grid_tile_count_x = 2;
    property.grid_tile_count_y = 1;
    property.no_data_value = -9999.0;

    alus::cuda::DeviceBuffer<float> device_values(values.size());
    CHECK_CUDA_ERR(
        cudaMemcpy(device_values.Get(), values.data(), values.size() * sizeof(float), cudaMemcpyHostToDevice));

    const alus::PointerHolder holder{1, device_values.Get(), 2, 2, 1};
    alus::cuda::DeviceBuffer<alus::PointerHolder> device_holder(1);
    CHECK_CUDA_ERR(cudaMemcpy(device_holder.Get(), &holder, sizeof(holder), cudaMemcpyHostToDevice));

    alus::cuda::DeviceBuffer<SamplesResult> device_result(1);
    GetSamplesKernel<<<1, 1>>>({device_holder.Get(), 1}, property, x0, x1, device_result.Get());
    CHECK_CUDA_ERR(cudaDeviceSynchronize());
    CHECK_CUDA_ERR(cudaGetLastError());

    SamplesResult result{};
    CHECK_CUDA_ERR(cudaMemcpy(&result, device_result.Get(), sizeof(result), cudaMemcpyDeviceToHost));
    return result;
}

TEST(CopDemCog30mCalc, ReadsCompleteNeighborhood) {
    const auto result = GetSamples(0, 1, {10.0F, 11.0F, 12.0F, 13.0F});

    EXPECT_EQ(result.all_valid, 1);
    EXPECT_THAT(result.samples, ::testing::ElementsAre(10.0, 11.0, 12.0, 13.0));
}

TEST(CopDemCog30mCalc, MissingNeighborForcesNoDataFallback) {
    const auto result = GetSamples(1, 2, {10.0F, 11.0F, 12.0F, 13.0F});

    EXPECT_EQ(result.all_valid, 0);
    EXPECT_TRUE(std::isnan(result.samples[0]));
    EXPECT_TRUE(std::isnan(result.samples[1]));
    EXPECT_EQ(result.samples[2], 13.0);
    EXPECT_TRUE(std::isnan(result.samples[3]));
}

TEST(CopDemCog30mCalc, RejectsExclusiveUpperTileBound) {
    const auto result = GetSamples(3, 4, {10.0F, 11.0F, 12.0F, 13.0F});

    EXPECT_EQ(result.all_valid, 0);
    EXPECT_TRUE(std::isnan(result.samples[0]));
    EXPECT_TRUE(std::isnan(result.samples[1]));
    EXPECT_TRUE(std::isnan(result.samples[3]));
}

TEST(CopDemCog30mCalc, MarksDemNoDataSampleInvalid) {
    const auto result = GetSamples(0, 1, {-9999.0F, 11.0F, 12.0F, 13.0F});

    EXPECT_EQ(result.all_valid, 0);
    EXPECT_TRUE(std::isnan(result.samples[0]));
}

}  // namespace
