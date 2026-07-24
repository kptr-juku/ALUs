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

#include "gmock/gmock.h"

#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "../include/thermal_noise_utils.h"
#include "s1tbx-commons/sentinel1_utils.h"
#include "snap-core/core/datamodel/metadata_element.h"
#include "test_expected_values.h"
#include "test_utils.h"

namespace test = alus::tnr::test;

namespace {

std::shared_ptr<alus::snapengine::MetadataElement> CreateNoiseVectorList(
    std::string_view lut_name, std::string_view pixel_values, std::string_view lut_values, int pixel_count,
    int lut_count) {
    const auto list = test::utils::CreateElement("noiseVectorList");
    const auto noise_vector = test::utils::CreateElement("noiseVector");
    noise_vector->SetAttributeString(alus::snapengine::AbstractMetadata::AZIMUTH_TIME, "2023-01-01T00:00:00");
    noise_vector->SetAttributeInt(alus::snapengine::AbstractMetadata::LINE, 0);

    const auto pixel = test::utils::CreateElement(alus::snapengine::AbstractMetadata::PIXEL);
    pixel->SetAttributeString(alus::snapengine::AbstractMetadata::PIXEL, pixel_values);
    pixel->SetAttributeInt(alus::snapengine::AbstractMetadata::COUNT, pixel_count);
    noise_vector->AddElement(pixel);

    const auto lut = test::utils::CreateElement(lut_name);
    lut->SetAttributeString(lut_name, lut_values);
    lut->SetAttributeInt(alus::snapengine::AbstractMetadata::COUNT, lut_count);
    noise_vector->AddElement(lut);
    list->AddElement(noise_vector);

    return list;
}

std::shared_ptr<alus::snapengine::MetadataElement> CreateCalibrationVectorList() {
    const auto list = test::utils::CreateElement(alus::snapengine::AbstractMetadata::CALIBRATION_VECTOR_LIST);
    const auto calibration_vector =
        test::utils::CreateElement(alus::snapengine::AbstractMetadata::CALIBRATION_VECTOR);
    calibration_vector->SetAttributeString(alus::snapengine::AbstractMetadata::AZIMUTH_TIME,
                                           "2023-01-01T00:00:00");
    calibration_vector->SetAttributeInt(alus::snapengine::AbstractMetadata::LINE, 0);

    const auto pixel = test::utils::CreateElement(alus::snapengine::AbstractMetadata::PIXEL);
    pixel->SetAttributeString(alus::snapengine::AbstractMetadata::PIXEL, "0\t 10\n20");
    pixel->SetAttributeInt(alus::snapengine::AbstractMetadata::COUNT, 3);
    calibration_vector->AddElement(pixel);

    const auto sigma = test::utils::CreateElement(alus::snapengine::AbstractMetadata::SIGMA_NOUGHT);
    sigma->SetAttributeString(alus::snapengine::AbstractMetadata::SIGMA_NOUGHT, "1.0\r\n2.0  \t3.0");
    sigma->SetAttributeInt(alus::snapengine::AbstractMetadata::COUNT, 3);
    calibration_vector->AddElement(sigma);
    list->AddElement(calibration_vector);

    return list;
}

std::shared_ptr<alus::snapengine::MetadataElement> CreateAzimuthNoiseVectorList(
    std::string_view line_values, std::string_view lut_values, int line_count, int lut_count) {
    const auto list = test::utils::CreateElement(alus::snapengine::AbstractMetadata::NOISE_AZIMUTH_VECTOR_LIST);
    const auto noise_vector = test::utils::CreateElement("noiseAzimuthVector");

    const auto line = test::utils::CreateElement(alus::snapengine::AbstractMetadata::LINE);
    line->SetAttributeString(alus::snapengine::AbstractMetadata::LINE, line_values);
    line->SetAttributeInt(alus::snapengine::AbstractMetadata::COUNT, line_count);
    noise_vector->AddElement(line);

    const auto lut = test::utils::CreateElement(alus::snapengine::AbstractMetadata::NOISE_AZIMUTH_LUT);
    lut->SetAttributeString(alus::snapengine::AbstractMetadata::NOISE_AZIMUTH_LUT, lut_values);
    lut->SetAttributeInt(alus::snapengine::AbstractMetadata::COUNT, lut_count);
    noise_vector->AddElement(lut);
    list->AddElement(noise_vector);

    return list;
}

class ThermalNoiseUtilsTest : public ::testing::Test {
protected:
    std::shared_ptr<alus::snapengine::MetadataElement> original_metadata_root_ = test::utils::CreateMetadataRoot();
};

TEST_F(ThermalNoiseUtilsTest, getAzimuthNoiseVectorListTest) {
    const auto iw1_vv_azimuth_list = original_metadata_root_->GetElement(alus::snapengine::AbstractMetadata::NOISE)
                                         ->GetElement(test::constants::IW1_VV_DATA.image_name)
                                         ->GetElement(alus::snapengine::AbstractMetadata::NOISE)
                                         ->GetElement(alus::snapengine::AbstractMetadata::NOISE_AZIMUTH_VECTOR_LIST);

    const auto computed_iw1_vv_azimuth_list = alus::tnr::GetAzimuthNoiseVectorList(iw1_vv_azimuth_list);
    test::utils::AssertAzimuthNoiseVectorsAreSame(test::expectedvalues::IW1_VV_NOISE_AZIMUTH_VECTOR_LIST,
                                                  computed_iw1_vv_azimuth_list);

    const auto iw2_vh_azimuth_list = original_metadata_root_->GetElement(alus::snapengine::AbstractMetadata::NOISE)
                                         ->GetElement(test::constants::IW2_VH_DATA.image_name)
                                         ->GetElement(alus::snapengine::AbstractMetadata::NOISE)
                                         ->GetElement(alus::snapengine::AbstractMetadata::NOISE_AZIMUTH_VECTOR_LIST);

    const auto computed_iw2_vh_azimuth_list = alus::tnr::GetAzimuthNoiseVectorList(iw2_vh_azimuth_list);
    test::utils::AssertAzimuthNoiseVectorsAreSame(test::expectedvalues::IW2_VH_NOISE_AZIMUTH_VECTOR_LIST,
                                                  computed_iw2_vh_azimuth_list);
}

TEST_F(ThermalNoiseUtilsTest, getNoiseVectorListTest) {
    const auto iw1_vv_noise_vector_list = original_metadata_root_->GetElement(alus::snapengine::AbstractMetadata::NOISE)
                                              ->GetElement(test::constants::IW1_VV_DATA.image_name)
                                              ->GetElement(alus::snapengine::AbstractMetadata::NOISE)
                                              ->GetElement(alus::snapengine::AbstractMetadata::NOISE_RANGE_VECTOR_LIST);
    const auto computed_iw1_vv_noise_list = alus::tnr::GetNoiseVectorList(iw1_vv_noise_vector_list);
    test::utils::AssertNoiseVectorListsAreSame(test::expectedvalues::IW1_VV_NOISE_VECTOR_LIST,
                                               computed_iw1_vv_noise_list);

    const auto iw2_vh_noise_vector_list = original_metadata_root_->GetElement(alus::snapengine::AbstractMetadata::NOISE)
                                              ->GetElement(test::constants::IW2_VH_DATA.image_name)
                                              ->GetElement(alus::snapengine::AbstractMetadata::NOISE)
                                              ->GetElement(alus::snapengine::AbstractMetadata::NOISE_RANGE_VECTOR_LIST);
    const auto computed_iw2_vh_noise_list = alus::tnr::GetNoiseVectorList(iw2_vh_noise_vector_list);
    test::utils::AssertNoiseVectorListsAreSame(test::expectedvalues::IW2_VH_NOISE_VECTOR_LIST,
                                               computed_iw2_vh_noise_list);
}

TEST(ThermalNoiseMetadataParsingTest, parsesLegacyNoiseLut) {
    const auto list = CreateNoiseVectorList("noiseLut", "0 10 20", "1.0 2.0 3.0", 3, 3);

    const auto vectors = alus::tnr::GetNoiseVectorList(list);

    ASSERT_THAT(vectors, ::testing::SizeIs(1));
    EXPECT_THAT(vectors.front().pixels, ::testing::ElementsAre(0, 10, 20));
    EXPECT_THAT(vectors.front().noise_lut, ::testing::ElementsAre(1.0F, 2.0F, 3.0F));
}

TEST(ThermalNoiseMetadataParsingTest, parsesNoiseRangeLut) {
    const auto list = CreateNoiseVectorList(alus::snapengine::AbstractMetadata::NOISE_RANGE_LUT, "0 10 20",
                                            "1.0 2.0 3.0", 3, 3);

    const auto vectors = alus::tnr::GetNoiseVectorList(list);

    ASSERT_THAT(vectors, ::testing::SizeIs(1));
    EXPECT_THAT(vectors.front().pixels, ::testing::ElementsAre(0, 10, 20));
    EXPECT_THAT(vectors.front().noise_lut, ::testing::ElementsAre(1.0F, 2.0F, 3.0F));
}

TEST(ThermalNoiseMetadataParsingTest, parsesGeneralWhitespace) {
    std::vector<int> integer_values;
    std::vector<float> float_values;

    alus::s1tbx::Sentinel1Utils::AddWhitespaceSeparatedValues(integer_values, " 0\t10\n20\r\n30 ");
    alus::s1tbx::Sentinel1Utils::AddWhitespaceSeparatedValues(float_values, " 1.0\t 2.0\n3.0\r\n4.0 ");

    EXPECT_THAT(integer_values, ::testing::ElementsAre(0, 10, 20, 30));
    EXPECT_THAT(float_values, ::testing::ElementsAre(1.0F, 2.0F, 3.0F, 4.0F));
}

TEST(ThermalNoiseMetadataParsingTest, rejectsMetadataCountMismatch) {
    const auto too_few = CreateNoiseVectorList(alus::snapengine::AbstractMetadata::NOISE_RANGE_LUT, "0 10 20",
                                               "1.0 2.0", 3, 3);
    const auto too_many = CreateNoiseVectorList(alus::snapengine::AbstractMetadata::NOISE_RANGE_LUT, "0 10 20",
                                                "1.0 2.0 3.0 4.0", 3, 3);

    EXPECT_THROW(alus::tnr::GetNoiseVectorList(too_few), std::runtime_error);
    EXPECT_THROW(alus::tnr::GetNoiseVectorList(too_many), std::runtime_error);
}

TEST(ThermalNoiseMetadataParsingTest, rejectsNoiseLutAndPixelCountMismatch) {
    const auto list = CreateNoiseVectorList(alus::snapengine::AbstractMetadata::NOISE_RANGE_LUT, "0 10 20",
                                            "1.0 2.0", 3, 2);

    EXPECT_THROW(alus::tnr::GetNoiseVectorList(list), std::runtime_error);
}

TEST(ThermalNoiseMetadataParsingTest, rejectsAzimuthLutAndLineCountMismatch) {
    const auto list = CreateAzimuthNoiseVectorList("0 10 20", "1.0 2.0", 3, 2);

    EXPECT_THROW(alus::tnr::GetAzimuthNoiseVectorList(list), std::runtime_error);
}

TEST(ThermalNoiseMetadataParsingTest, parsesCalibrationLutWithGeneralWhitespace) {
    const auto vectors =
        alus::s1tbx::Sentinel1Utils::GetCalibrationVectors(CreateCalibrationVectorList(), true, false, false, false);

    ASSERT_THAT(vectors, ::testing::SizeIs(1));
    EXPECT_THAT(vectors.front().pixels, ::testing::ElementsAre(0, 10, 20));
    EXPECT_THAT(vectors.front().sigma_nought, ::testing::ElementsAre(1.0F, 2.0F, 3.0F));
}

TEST(ThermalNoiseMetadataParsingTest, rejectsCalibrationMetadataCountMismatch) {
    const auto list = CreateCalibrationVectorList();
    const auto sigma = list->GetElements().front()->GetElement(alus::snapengine::AbstractMetadata::SIGMA_NOUGHT);
    sigma->SetAttributeInt(alus::snapengine::AbstractMetadata::COUNT, 2);

    EXPECT_THROW(alus::s1tbx::Sentinel1Utils::GetCalibrationVectors(list, true, false, false, false),
                 std::runtime_error);
}

TEST_F(ThermalNoiseUtilsTest, fillTimeMapsWithT0AndDeltaTStest) {
    alus::tnr::TimeMaps iw1_vv_time_maps;
    alus::tnr::FillTimeMapsWithT0AndDeltaTS(test::constants::IW1_VV_DATA.image_name, original_metadata_root_,
                                                     iw1_vv_time_maps);
    test::utils::AssertTimeMapsAreSame(test::expectedvalues::IW1_VV_TIME_MAPS, iw1_vv_time_maps);

    alus::tnr::TimeMaps iw2_vh_time_maps;
    alus::tnr::FillTimeMapsWithT0AndDeltaTS(test::constants::IW2_VH_DATA.image_name, original_metadata_root_,
                                                     iw2_vh_time_maps);
    test::utils::AssertTimeMapsAreSame(test::expectedvalues::IW2_VH_TIME_MAPS, iw2_vh_time_maps);
}

TEST_F(ThermalNoiseUtilsTest, getBurstRangeVectorTest) {
    const std::vector<std::pair<int, int>> center_line_to_burst_index{
        {12775, 9}, {751, 1}, {2254, 2}, {5260, 4}, {6763, 5}, {8266, 6}, {9769, 7}, {11272, 8}, {3757, 3},
    };

    for (const auto& [burst_center_line, vector_index] : center_line_to_burst_index) {
        const auto computed_vector =
            alus::tnr::GetBurstRangeVector(burst_center_line, test::constants::NOISE_VECTORS);
        test::utils::AssertNoiseVectorsAreSame(test::constants::NOISE_VECTORS.at(vector_index), computed_vector);
    }
}
}  // namespace
