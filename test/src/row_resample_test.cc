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
#include <limits>
#include <vector>

#include "gmock/gmock.h"

#include "row_resample.h"

namespace {

TEST(RowResample, ReplicatesLastInputPixelWithoutReadingPastLine) {
    std::vector<float> input{1.0F, 2.0F, 3.0F, std::numeric_limits<float>::quiet_NaN()};
    std::vector<float> output(6);

    alus::rowresample::FillLineFrom(input.data(), input.size() - 1, output.data(), output.size());

    EXPECT_THAT(output, ::testing::ElementsAre(1.0F, 1.0F, 2.0F, 2.0F, 3.0F, 3.0F));
}

}  // namespace
