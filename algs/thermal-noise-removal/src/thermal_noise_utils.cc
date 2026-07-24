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

#include "thermal_noise_utils.h"

#include <boost/algorithm/string.hpp>

#include <driver_types.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <string>
#include <string_view>

#include "alus_log.h"
#include "s1tbx-commons/sentinel1_utils.h"
#include "snap-core/core/datamodel/metadata_element.h"
#include "snap-engine-utilities/engine-utilities/datamodel/metadata/abstract_metadata.h"
#include "snap-engine-utilities/engine-utilities/eo/constants.h"
#include "thermal_noise_data_structures.h"
#include "thermal_noise_kernel.h"
#include "time_maps.h"

namespace alus::tnr {
namespace {

// IPF 2.9.0 introduced noiseRangeLut; older SAFE products store the same values under noiseLut.
constexpr std::string_view LEGACY_NOISE_LUT{"noiseLut"};
constexpr std::string_view SWATH_MERGING{"swathMerging"};
constexpr std::string_view SWATH_MERGE_LIST{"swathMergeList"};
constexpr std::string_view SWATH_BOUNDS_LIST{"swathBoundsList"};

void ValidateMetadataCount(std::string_view field, int declared_count, std::size_t parsed_count) {
    if (declared_count < 0 || parsed_count != static_cast<std::size_t>(declared_count)) {
        throw std::runtime_error(std::string(field) + " metadata count is " + std::to_string(declared_count) +
                                 ", but " + std::to_string(parsed_count) + " values were parsed");
    }
}

}  // namespace

ThermalNoiseInfo GetThermalNoiseInfoForBursts(
    std::string_view polarisation, std::string_view sub_swath,
    const std::shared_ptr<snapengine::MetadataElement>& origin_metadata_root) {
    ThermalNoiseInfo thermal_noise_info;

    const auto root_noise_element = origin_metadata_root->GetElement(snapengine::AbstractMetadata::NOISE);

    for (const auto& list_element : root_noise_element->GetElements()) {
        const auto image_name = list_element->GetName();
        if (boost::algorithm::icontains(image_name, polarisation) &&
            boost::algorithm::icontains(image_name, sub_swath)) {
            const auto noise_element = list_element->GetElement(snapengine::AbstractMetadata::NOISE);

            thermal_noise_info.noise_azimuth_vectors = GetAzimuthNoiseVectorList(
                noise_element->GetElement(snapengine::AbstractMetadata::NOISE_AZIMUTH_VECTOR_LIST));
            thermal_noise_info.noise_range_vectors =
                GetNoiseVectorList(noise_element->GetElement(snapengine::AbstractMetadata::NOISE_RANGE_VECTOR_LIST));

            const auto ads_header = noise_element->GetElement(snapengine::AbstractMetadata::ADS_HEADER);
            if (!ads_header) {
                throw std::runtime_error("Noise metadata is missing adsHeader required for TOPS SLC vector alignment");
            }
            const auto start_time =
                s1tbx::Sentinel1Utils::GetTime(ads_header, snapengine::AbstractMetadata::START_TIME)->GetMjd();
            const auto vectors_to_skip = GetClosestNoiseVectorIndex(start_time, thermal_noise_info.noise_range_vectors);
            if (vectors_to_skip > 0) {
                LOGW << "TOPS SLC noise metadata starts after " << vectors_to_skip
                     << " range vector(s); aligning burst indices to adsHeader.startTime as Microwave Toolbox does";
            }
            thermal_noise_info.burst_to_range_vector_map.assign(
                thermal_noise_info.noise_range_vectors.begin() + static_cast<std::ptrdiff_t>(vectors_to_skip),
                thermal_noise_info.noise_range_vectors.end());
            break;
        }
    }

    if (thermal_noise_info.burst_to_range_vector_map.empty()) {
        throw std::runtime_error("No noise range vectors found for TOPS SLC thermal noise removal");
    }

    for (const auto& image_element :
         origin_metadata_root->GetElement(snapengine::AbstractMetadata::ANNOTATION)->GetElements()) {
        const auto image_name = image_element->GetName();
        if (boost::algorithm::icontains(image_name, polarisation) &&
            boost::algorithm::icontains(image_name, sub_swath)) {
            const auto swath_timing_element = image_element->GetElement(snapengine::AbstractMetadata::PRODUCT)
                                                  ->GetElement(snapengine::AbstractMetadata::SWATH_TIMING);

            thermal_noise_info.lines_per_burst =
                swath_timing_element->GetAttributeInt(snapengine::AbstractMetadata::LINES_PER_BURST);
            break;
        }
    }

    if (thermal_noise_info.lines_per_burst <= 0) {
        throw std::runtime_error("Invalid linesPerBurst metadata for TOPS SLC thermal noise removal");
    }

    return thermal_noise_info;
}

ThermalNoiseInfo GetThermalNoiseInfoForGrd(std::string_view polarisation,
                                           const std::shared_ptr<snapengine::MetadataElement>& origin_metadata_root) {
    ThermalNoiseInfo thermal_noise_info;

    const auto root_noise_element = origin_metadata_root->GetElement(snapengine::AbstractMetadata::NOISE);

    for (const auto& list_element : root_noise_element->GetElements()) {
        const auto image_name = list_element->GetName();
        if (boost::algorithm::icontains(image_name, polarisation)) {
            const auto noise_element = list_element->GetElement(snapengine::AbstractMetadata::NOISE);

            thermal_noise_info.noise_azimuth_vectors = GetAzimuthNoiseVectorList(
                noise_element->GetElement(snapengine::AbstractMetadata::NOISE_AZIMUTH_VECTOR_LIST));
            thermal_noise_info.noise_range_vectors =
                GetNoiseVectorList(noise_element->GetElement(snapengine::AbstractMetadata::NOISE_RANGE_VECTOR_LIST));
        }
    }

    return thermal_noise_info;
}

void FillTimeMapsWithT0AndDeltaTS(const std::string_view& image_name,
                                  const std::shared_ptr<snapengine::MetadataElement>& origin_metadata_root,
                                  TimeMaps& time_maps) {
    const auto annotation_elements =
        origin_metadata_root->GetElement(snapengine::AbstractMetadata::ANNOTATION)->GetElements();

    for (const auto& annotation_element : annotation_elements) {
        if (annotation_element->GetName().find(image_name) != std::string::npos) {
            const auto image_information_element = annotation_element->GetElement(snapengine::AbstractMetadata::PRODUCT)
                                                       ->GetElement(snapengine::AbstractMetadata::IMAGE_ANNOTATION)
                                                       ->GetElement(snapengine::AbstractMetadata::IMAGE_INFORMATION);

            const auto t_0 = s1tbx::Sentinel1Utils::GetTime(image_information_element,
                                                            snapengine::AbstractMetadata::PRODUCT_FIRST_LINE_UTC_TIME)
                                 ->GetMjd();
            const auto image_name_key = std::string(image_name);
            time_maps.t_0_map.insert_or_assign(image_name_key, t_0);

            const auto delta_ts =
                image_information_element->GetAttributeDouble(snapengine::AbstractMetadata::AZIMUTH_TIME_INTERVAL) /
                snapengine::eo::constants::SECONDS_IN_DAY;
            time_maps.delta_t_map.insert_or_assign(image_name_key, delta_ts);

            const auto product_element = annotation_element->GetElement(snapengine::AbstractMetadata::PRODUCT);
            const auto swath_merging_element = product_element->GetElement(SWATH_MERGING);
            if (swath_merging_element) {
                const auto swath_merge_list_element = swath_merging_element->GetElement(SWATH_MERGE_LIST);
                if (swath_merge_list_element) {
                    for (const auto& swath_merge_element : swath_merge_list_element->GetElements()) {
                        if (!swath_merge_element->ContainsAttribute(snapengine::AbstractMetadata::SWATH)) {
                            continue;
                        }
                        const auto swath_bounds_list_element = swath_merge_element->GetElement(SWATH_BOUNDS_LIST);
                        if (!swath_bounds_list_element) {
                            continue;
                        }
                        const auto swath_bounds = swath_bounds_list_element->GetElements();
                        if (swath_bounds.empty()) {
                            continue;
                        }

                        const auto first_line = swath_bounds.front()->GetAttributeInt(
                            snapengine::AbstractMetadata::FIRST_AZIMUTH_LINE);
                        const auto last_line =
                            swath_bounds.back()->GetAttributeInt(snapengine::AbstractMetadata::LAST_AZIMUTH_LINE);
                        time_maps.swath_start_end_times_map.insert_or_assign(
                            swath_merge_element->GetAttributeString(snapengine::AbstractMetadata::SWATH),
                            std::vector<double>{t_0 + first_line * delta_ts, t_0 + last_line * delta_ts});
                    }
                }
            }

            return;
        }
    }
}

std::vector<s1tbx::NoiseAzimuthVector> GetAzimuthNoiseVectorList(
    const std::shared_ptr<snapengine::MetadataElement>& azimuth_noise_vector_list_element) {
    const auto elements_list = azimuth_noise_vector_list_element->GetElements();
    std::vector<s1tbx::NoiseAzimuthVector> noise_vector_list;
    noise_vector_list.reserve(elements_list.size());

    for (const auto& noise_vector_element : elements_list) {
        const auto line_element = noise_vector_element->GetElement(snapengine::AbstractMetadata::LINE);
        const auto line_attribute = line_element->GetAttributeString(snapengine::AbstractMetadata::LINE);
        const auto count = line_element->GetAttributeInt(snapengine::AbstractMetadata::COUNT);
        std::vector<int> line_vector;
        s1tbx::Sentinel1Utils::AddWhitespaceSeparatedValues(line_vector, line_attribute);
        ValidateMetadataCount(snapengine::AbstractMetadata::LINE, count, line_vector.size());

        const auto noise_lut_element =
            noise_vector_element->GetElement(snapengine::AbstractMetadata::NOISE_AZIMUTH_LUT);
        const auto noise_lut_attribute =
            noise_lut_element->GetAttributeString(snapengine::AbstractMetadata::NOISE_AZIMUTH_LUT);
        const auto noise_lut_count = noise_lut_element->GetAttributeInt(snapengine::AbstractMetadata::COUNT);
        std::vector<float> noise_lut_vector;
        s1tbx::Sentinel1Utils::AddWhitespaceSeparatedValues(noise_lut_vector, noise_lut_attribute);
        ValidateMetadataCount(snapengine::AbstractMetadata::NOISE_AZIMUTH_LUT, noise_lut_count,
                              noise_lut_vector.size());
        if (noise_lut_count != count) {
            throw std::runtime_error("noiseAzimuthLut metadata count does not match line count");
        }
        if (line_vector.empty()) {
            throw std::runtime_error("Noise azimuth vector contains no line/LUT values");
        }

        const auto swath = noise_vector_element->ContainsAttribute(snapengine::AbstractMetadata::SWATH)
                               ? noise_vector_element->GetAttributeString(snapengine::AbstractMetadata::SWATH)
                               : "";
        const auto first_azimuth_line =
            noise_vector_element->ContainsAttribute(snapengine::AbstractMetadata::FIRST_AZIMUTH_LINE)
                ? std::stoi(noise_vector_element->GetAttributeString(snapengine::AbstractMetadata::FIRST_AZIMUTH_LINE))
                : -1;
        const auto last_azimuth_line =
            noise_vector_element->ContainsAttribute(snapengine::AbstractMetadata::LAST_AZIMUTH_LINE)
                ? std::stoi(noise_vector_element->GetAttributeString(snapengine::AbstractMetadata::LAST_AZIMUTH_LINE))
                : -1;
        const auto first_range_sample =
            noise_vector_element->ContainsAttribute(snapengine::AbstractMetadata::FIRST_RANGE_SAMPLE)
                ? std::stoi(noise_vector_element->GetAttributeString(snapengine::AbstractMetadata::FIRST_RANGE_SAMPLE))
                : -1;
        const auto last_range_sample =
            noise_vector_element->ContainsAttribute(snapengine::AbstractMetadata::LAST_RANGE_SAMPLE)
                ? std::stoi(noise_vector_element->GetAttributeString(snapengine::AbstractMetadata::LAST_RANGE_SAMPLE))
                : -1;

        noise_vector_list.push_back({swath, first_azimuth_line, first_range_sample, last_azimuth_line,
                                     last_range_sample, line_vector, noise_lut_vector});
    }

    return noise_vector_list;
}

std::vector<s1tbx::NoiseVector> GetNoiseVectorList(
    const std::shared_ptr<snapengine::MetadataElement>& noise_vector_list_element) {
    const auto list = noise_vector_list_element->GetElements();
    std::vector<s1tbx::NoiseVector> noise_vector_list;
    noise_vector_list.reserve(list.size());

    for (const auto& noise_vector_element : list) {
        const auto time =
            s1tbx::Sentinel1Utils::GetTime(noise_vector_element, snapengine::AbstractMetadata::AZIMUTH_TIME)->GetMjd();
        const auto line = std::stoi(noise_vector_element->GetAttributeString(snapengine::AbstractMetadata::LINE));

        const auto pixel_element = noise_vector_element->GetElement(snapengine::AbstractMetadata::PIXEL);
        const auto pixel_attribute = pixel_element->GetAttributeString(snapengine::AbstractMetadata::PIXEL);
        const auto count = pixel_element->GetAttributeInt(snapengine::AbstractMetadata::COUNT);
        std::vector<int> pixel_vector;
        s1tbx::Sentinel1Utils::AddWhitespaceSeparatedValues(pixel_vector, pixel_attribute);
        ValidateMetadataCount(snapengine::AbstractMetadata::PIXEL, count, pixel_vector.size());

        auto noise_lut_element = noise_vector_element->GetElement(LEGACY_NOISE_LUT);
        if (!noise_lut_element) {
            noise_lut_element = noise_vector_element->GetElement(snapengine::AbstractMetadata::NOISE_RANGE_LUT);
        }
        if (!noise_lut_element) {
            throw std::runtime_error("Noise vector is missing noiseLut/noiseRangeLut metadata element");
        }

        std::string noise_lut_attribute;
        if (noise_lut_element->ContainsAttribute(LEGACY_NOISE_LUT)) {
            noise_lut_attribute = noise_lut_element->GetAttributeString(LEGACY_NOISE_LUT);
        } else if (noise_lut_element->ContainsAttribute(snapengine::AbstractMetadata::NOISE_RANGE_LUT)) {
            noise_lut_attribute =
                noise_lut_element->GetAttributeString(snapengine::AbstractMetadata::NOISE_RANGE_LUT);
        } else {
            throw std::runtime_error("Noise LUT element is missing noiseLut/noiseRangeLut metadata attribute");
        }

        const auto noise_lut_count = noise_lut_element->GetAttributeInt(snapengine::AbstractMetadata::COUNT);
        std::vector<float> noise_lut_vector;
        s1tbx::Sentinel1Utils::AddWhitespaceSeparatedValues(noise_lut_vector, noise_lut_attribute);
        ValidateMetadataCount(noise_lut_element->GetName(), noise_lut_count, noise_lut_vector.size());
        if (noise_lut_count != count) {
            throw std::runtime_error("Noise LUT metadata count does not match pixel count");
        }
        if (pixel_vector.empty()) {
            throw std::runtime_error("Noise range vector contains no pixel/LUT values");
        }
        if (pixel_vector.size() == 1) {
            LOGW << "Noise range vector at line " << line
                 << " contains one LUT value; using it as a constant as Microwave Toolbox does";
        }

        noise_vector_list.push_back({time, line, pixel_vector, noise_lut_vector});
    }

    return noise_vector_list;
}
s1tbx::NoiseVector GetBurstRangeVector(int burst_center_line,
                                       const std::vector<s1tbx::NoiseVector>& noise_range_vectors) {
    size_t closest{0};
    for (size_t i = 1; i < noise_range_vectors.size(); i++) {
        if (std::abs(burst_center_line - noise_range_vectors.at(i).line) <
            std::abs(burst_center_line - noise_range_vectors.at(closest).line)) {
            closest = i;
        }
    }
    return noise_range_vectors.at(closest);
}

size_t GetClosestNoiseVectorIndex(double azimuth_time,
                                  const std::vector<s1tbx::NoiseVector>& noise_range_vectors) {
    if (noise_range_vectors.empty()) {
        throw std::runtime_error("Cannot select a noise range vector from an empty list");
    }

    size_t closest{0};
    for (size_t i = 1; i < noise_range_vectors.size(); i++) {
        if (std::abs(azimuth_time - noise_range_vectors.at(i).time_mjd) <
            std::abs(azimuth_time - noise_range_vectors.at(closest).time_mjd)) {
            closest = i;
        }
    }
    return closest;
}
size_t GetLineIndex(int line, const std::vector<int>& lines) {
    if (lines.size() < 2) {
        return 0;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        if (line < lines.at(i)) {
            return i > 0 ? i - 1 : 0;
        }
    }

    return lines.size() - 2;
}
device::Matrix<double> BuildNoiseLutForTOPSSLC(Rectangle tile, const ThermalNoiseInfo& thermal_noise_info,
                                               ThreadData* thread_data) {
    const auto first_burst_index = tile.y / thermal_noise_info.lines_per_burst;
    const auto last_burst_index = (tile.y + tile.height - 1) / thermal_noise_info.lines_per_burst;
    if (thermal_noise_info.burst_to_range_vector_map.empty() || first_burst_index < 0 ||
        last_burst_index >= static_cast<int>(thermal_noise_info.burst_to_range_vector_map.size())) {
        const auto last_available_burst = thermal_noise_info.burst_to_range_vector_map.empty()
                                              ? -1
                                              : static_cast<int>(thermal_noise_info.burst_to_range_vector_map.size()) - 1;
        throw std::runtime_error("TOPS SLC tile requires noise range vector for burst " +
                                 std::to_string(last_burst_index) + ", but metadata provides vectors through burst " +
                                 std::to_string(last_available_burst));
    }

    // INTERPOLATE NOISE AZIMUTH KERNEL
    const auto d_azimuth_vector = thermal_noise_info.noise_azimuth_vectors.at(0).ToDeviceVector();
    const auto starting_line_index = GetLineIndex(tile.y, thermal_noise_info.noise_azimuth_vectors.at(0).lines);
    const auto interpolated_azimuth_vector = LaunchInterpolateNoiseAzimuthVectorKernel(
        d_azimuth_vector, tile.y, tile.y + tile.height + 1, starting_line_index, thread_data->stream);

    // GET SAMPLE INDICES
    std::vector<s1tbx::DeviceNoiseVector> h_burst_to_range_map(thermal_noise_info.burst_to_range_vector_map.size());
    std::transform(std::begin(thermal_noise_info.burst_to_range_vector_map),
                   std::end(thermal_noise_info.burst_to_range_vector_map), std::begin(h_burst_to_range_map),
                   [](auto& vector) { return vector.ToDeviceVector(); });
    cuda::KernelArray<s1tbx::DeviceNoiseVector> d_burst_to_range_map{nullptr, h_burst_to_range_map.size()};
    CHECK_CUDA_ERR(cudaMalloc(&d_burst_to_range_map.array, d_burst_to_range_map.ByteSize()));
    CHECK_CUDA_ERR(cudaMemcpy(d_burst_to_range_map.array, h_burst_to_range_map.data(), d_burst_to_range_map.ByteSize(),
                              cudaMemcpyHostToDevice));

    const auto d_burst_indices = CalculateBurstIndices(tile, thermal_noise_info.lines_per_burst, thread_data);
    const auto d_sample_indices =
        LaunchGetSampleIndexKernel(tile, d_burst_to_range_map, d_burst_indices, thread_data->stream);

    // INTERPOLATE NOISE RANGE VECTOR
    const auto index_to_interpolated_range_vector_map = LaunchInterpolateNoiseRangeVectorsKernel(
        tile, d_burst_indices, d_sample_indices, d_burst_to_range_map, thread_data->stream);
    // Calculate noise matrix
    const auto noise_matrix =
        CalculateNoiseMatrix(tile, thermal_noise_info.lines_per_burst, interpolated_azimuth_vector,
                             index_to_interpolated_range_vector_map, thread_data->stream);

    // DEALLOCATE MEMORY
    CHECK_CUDA_ERR(cudaFree(d_azimuth_vector.lines.array));
    CHECK_CUDA_ERR(cudaFree(d_azimuth_vector.noise_azimuth_lut.array));
    CHECK_CUDA_ERR(cudaFree(interpolated_azimuth_vector.array));
    CHECK_CUDA_ERR(cudaFree(d_sample_indices.array));
    for (auto& vector : h_burst_to_range_map) {
        CHECK_CUDA_ERR(cudaFree(vector.pixels.array));
        CHECK_CUDA_ERR(cudaFree(vector.noise_lut.array));
    }
    CHECK_CUDA_ERR(cudaFree(d_burst_to_range_map.array));
    CHECK_CUDA_ERR(cudaFree(d_burst_indices.array));
    device::DestroyBurstIndexToInterpolatedRangeVectorMap(index_to_interpolated_range_vector_map);

    return noise_matrix;
}

device::Matrix<double> BuildNoiseLutForTOPSGRD(Rectangle tile, const ThermalNoiseInfo& thermal_noise_info,
                                               ThreadData* thread_data) {
    if (thermal_noise_info.time_maps.t_0_map.empty() || thermal_noise_info.time_maps.delta_t_map.empty()) {
        throw std::runtime_error("Missing first-line time or azimuth interval metadata for GRD thermal noise removal");
    }

    const auto x_max = tile.x + tile.width - 1;
    const auto y_max = tile.y + tile.height - 1;
    bool has_data{false};
    // Although it is GRD, the noise azimuth vectors are segregated by the originating swath in the metadata.
    // Hence 3 vectors, each for a subswath.
    std::vector<std::vector<double>> noise_matrix(tile.height);
    for (auto& v : noise_matrix) {
        v.resize(tile.width);
    }
    for (auto& nav : thermal_noise_info.noise_azimuth_vectors) {
        const auto nx0 = std::max(tile.x, nav.first_range_sample);
        const auto nx_max = std::min(x_max, nav.last_range_sample);
        const auto ny0 = std::max(tile.y, nav.first_azimuth_line);
        const auto ny_max = std::min(y_max, nav.last_azimuth_line);

        if (nx0 >= nx_max || ny0 >= ny_max) {
            continue;
        }

        has_data = true;
        // Contains a single polarisation data only for GRD. If needed more can be added later.
        const auto first_line_time = thermal_noise_info.time_maps.t_0_map.begin()->second;
        const auto line_time_interval = thermal_noise_info.time_maps.delta_t_map.begin()->second;
        const auto start_azim_time = first_line_time + nav.first_azimuth_line * line_time_interval;
        const auto end_azim_time = first_line_time + nav.last_azimuth_line * line_time_interval;
        const auto swath_times = thermal_noise_info.time_maps.swath_start_end_times_map.find(nav.swath);
        const std::vector<double> empty_swath_times;
        const auto& swath_start_end_times =
            swath_times != thermal_noise_info.time_maps.swath_start_end_times_map.end() ? swath_times->second
                                                                                        : empty_swath_times;
        const auto noise_vector_indices = DetermineNoiseVectorIndices(
            start_azim_time, end_azim_time, thermal_noise_info.noise_range_vectors, swath_start_end_times);
        const bool first_tile_for_block = tile.x <= nav.first_range_sample && nav.first_range_sample <= x_max &&
                                          tile.y <= nav.first_azimuth_line && nav.first_azimuth_line <= y_max;
        if (noise_vector_indices.empty()) {
            if (first_tile_for_block) {
                LOGW << std::setprecision(15) << "No valid noise range vector found for " << nav.swath
                     << " azimuth block lines [" << nav.first_azimuth_line << ", " << nav.last_azimuth_line
                     << "] and times [" << start_azim_time << ", " << end_azim_time
                     << "]; metadata is insufficient, so the thermal-noise LUT remains zero in this block";
            }
            continue;
        }
        if (noise_vector_indices.size() == 1) {
            const auto selected_index = noise_vector_indices.front();
            const auto selected_time = thermal_noise_info.noise_range_vectors.at(selected_index).time_mjd;
            if (first_tile_for_block && (selected_time < start_azim_time || selected_time > end_azim_time)) {
                LOGW << std::setprecision(15) << "No noise range vector falls inside " << nav.swath
                     << " azimuth block lines [" << nav.first_azimuth_line << ", " << nav.last_azimuth_line
                     << "] and times [" << start_azim_time << ", " << end_azim_time << "]; using nearest in-swath "
                     << "vector " << selected_index << " at " << selected_time
                     << " as Microwave Toolbox does, with range noise held constant across the block";
            }
        }

        const auto interpolated_range_value_count = nx_max - nx0 + 1;
        std::vector<std::vector<double>> interpolated_range_vectors(noise_vector_indices.size());
        std::vector<double> noise_range_vector_azimuth_times(noise_vector_indices.size());
        for (int i{0}; i < static_cast<int>(noise_vector_indices.size()); i++) {
            const auto& noise_range_vector = thermal_noise_info.noise_range_vectors.at(noise_vector_indices.at(i));
            noise_range_vector_azimuth_times[i] = noise_range_vector.time_mjd;

            interpolated_range_vectors.at(i) = std::vector<double>(interpolated_range_value_count);
            FillRangeNoiseWithInterpolatedValues(noise_range_vector, nx0, nx_max, interpolated_range_vectors.at(i));
        }

        std::vector<double> interpolated_azimuth_vector(ny_max - ny0 + 1);
        FillAzimuthNoiseVectorWithInterpolatedValues(nav, ny0, ny_max, interpolated_azimuth_vector);
        ComputeNoiseMatrix(tile.x, tile.y, nx0, nx_max, ny0, ny_max, first_line_time, line_time_interval,
                           noise_range_vector_azimuth_times, interpolated_range_vectors, interpolated_azimuth_vector,
                           noise_matrix);
    }

    if (!has_data) {
        LOGW << "No noise azimuth metadata block overlaps tile x:" << tile.x << " y:" << tile.y
             << " w:" << tile.width << " h:" << tile.height
             << "; the thermal-noise LUT remains zero for this tile";
    }

    const auto noise_matrix_dev =
        device::CreateKernelMatrix<double>(tile.width, tile.height, noise_matrix, thread_data->stream);
    return noise_matrix_dev;
}

cuda::KernelArray<int> CalculateBurstIndices(Rectangle tile, int lines_per_burst, ThreadData* thread_data) {
    if (lines_per_burst <= 0) {
        throw std::runtime_error(
            "CalculateBurstIndices failed while building the TOPS SLC thermal-noise LUT: annotation "
            "swathTiming.linesPerBurst must be positive, but was " +
            std::to_string(lines_per_burst) +
            ". The selected subswath or split product may have missing, malformed, or inconsistent burst metadata.");
    }

    const auto first_burst_index = tile.y / lines_per_burst;
    const auto last_burst_index = (tile.y + tile.height - 1) / lines_per_burst;
    std::vector<int> burst_indices;
    burst_indices.reserve(last_burst_index - first_burst_index + 1);
    for (auto burst_index = first_burst_index; burst_index <= last_burst_index; burst_index++) {
        burst_indices.emplace_back(burst_index);
    }
    (void)thread_data;
    cuda::KernelArray<int> d_burst_indices{nullptr, burst_indices.size()};
    CHECK_CUDA_ERR(cudaMalloc(&d_burst_indices.array, d_burst_indices.ByteSize()));
    CHECK_CUDA_ERR(
        cudaMemcpy(d_burst_indices.array, burst_indices.data(), d_burst_indices.ByteSize(), cudaMemcpyHostToDevice));

    return d_burst_indices;
}

std::vector<size_t> DetermineNoiseVectorIndices(double start_az_time, double end_az_time,
                                                 const std::vector<s1tbx::NoiseVector>& noise_range,
                                                 const std::vector<double>& swath_start_end_times) {
    std::vector<size_t> results;
    for (size_t i{0}; i < noise_range.size(); i++) {
        if (noise_range.at(i).time_mjd >= start_az_time && noise_range.at(i).time_mjd <= end_az_time) {
            results.push_back(i);
        }
    }

    if (results.empty() && swath_start_end_times.size() == 2) {
        const auto block_centre_time = (start_az_time + end_az_time) / 2.0;
        size_t closest = noise_range.size();
        for (size_t i = 0; i < noise_range.size(); i++) {
            const auto azimuth_time = noise_range.at(i).time_mjd;
            if (azimuth_time < swath_start_end_times.front() || azimuth_time > swath_start_end_times.back()) {
                continue;
            }
            if (closest == noise_range.size() ||
                std::abs(block_centre_time - azimuth_time) <
                    std::abs(block_centre_time - noise_range.at(closest).time_mjd)) {
                closest = i;
            }
        }
        if (closest != noise_range.size()) {
            results.push_back(closest);
        }
    }

    return results;
}

inline double InterpolateNoise(int p1, int p2, double noise1, double noise2, int sample_index) {
    if (p1 == p2) {
        return 0.0;
    }
    return noise1 + (static_cast<double>(sample_index - p1) / static_cast<double>(p2 - p1)) * (noise2 - noise1);
}

inline double InterpolateNoiseByTime(double t1, double t2, double noise1, double noise2, double azimuth_time) {
    if (t1 == t2) {
        return 0.0;
    }
    return noise1 + ((azimuth_time - t1) / (t2 - t1)) * (noise2 - noise1);
}

void FillRangeNoiseWithInterpolatedValues(const s1tbx::NoiseVector& nv, int first_range_sample, int last_range_sample,
                                          std::vector<double>& to_compute) {
    const int nv_pix_len{static_cast<int>(nv.pixels.size())};
    if (nv_pix_len == 0) {
        throw std::runtime_error(std::string(__FUNCTION__) + " received an empty noise range vector");
    }
    if (nv_pix_len == 1) {
        std::fill(to_compute.begin(), to_compute.end(), nv.noise_lut.front());
        return;
    }

    size_t computed_index{0};
    auto sample_index = GetSampleIndex(first_range_sample, nv.pixels);
    for (int s{first_range_sample}; s <= last_range_sample; s++) {
        if (s > nv.pixels.at(sample_index + 1) && sample_index < (nv_pix_len - 2)) {
            sample_index++;
        }

        to_compute.at(computed_index++) =
            InterpolateNoise(nv.pixels.at(sample_index), nv.pixels.at(sample_index + 1), nv.noise_lut.at(sample_index),
                             nv.noise_lut.at(sample_index + 1), s);
    }
}

void FillAzimuthNoiseVectorWithInterpolatedValues(const s1tbx::NoiseAzimuthVector& v, int first_azimuth_line,
                                                  int last_azimuth_line, std::vector<double>& to_compute) {
    const int nv_pix_len{static_cast<int>(v.lines.size())};
    if (nv_pix_len < 2) {
        for (int line = first_azimuth_line; line <= last_azimuth_line; line++) {
            to_compute.at(line - first_azimuth_line) = v.noise_azimuth_lut.front();
        }
    } else {
        int line_index = GetSampleIndex(first_azimuth_line, v.lines);
        for (int line = first_azimuth_line; line <= last_azimuth_line; line++) {
            if (line > v.lines.at(line_index + 1) && line_index < nv_pix_len - 2) {
                line_index++;
            }

            to_compute.at(line - first_azimuth_line) =
                InterpolateNoise(v.lines.at(line_index), v.lines.at(line_index + 1), v.noise_azimuth_lut.at(line_index),
                                 v.noise_azimuth_lut.at(line_index + 1), line);
        }
    }
}

void ComputeNoiseMatrix(int tile_offset_x, int tile_offset_y, int nx0, int nx_max, int ny0, int ny_max,
                        double first_line_time, double line_time_interval,
                        const std::vector<double>& noise_range_vector_azimuth_times,
                        const std::vector<std::vector<double>>& interpolated_range_vectors,
                        const std::vector<double>& interpolated_azimuth_vector,
                        std::vector<std::vector<double>>& values) {
    if (noise_range_vector_azimuth_times.size() == 1) {
        for (int x = nx0; x <= nx_max; x++) {
            const int xx = x - nx0;
            for (int y = ny0; y <= ny_max; y++) {
                values.at(y - tile_offset_y).at(x - tile_offset_x) =
                    interpolated_azimuth_vector.at(y - ny0) * interpolated_range_vectors.front().at(xx);
            }
        }

    } else {
        const auto first_azimuth_time = first_line_time + line_time_interval * ny0;
        size_t first_time_index{0};
        for (size_t i = 0; i < noise_range_vector_azimuth_times.size(); i++) {
            if (first_azimuth_time < noise_range_vector_azimuth_times.at(i)) {
                first_time_index = i > 0 ? i - 1 : 0;
                break;
            }
            first_time_index = noise_range_vector_azimuth_times.size() - 2;
        }

        for (int x = nx0; x <= nx_max; x++) {
            const int xx = x - nx0;
            auto time_index = first_time_index;

            for (int y = ny0; y <= ny_max; y++) {
                const auto azimuth_time = first_line_time + line_time_interval * y;
                if (azimuth_time > noise_range_vector_azimuth_times[time_index + 1] &&
                    time_index < noise_range_vector_azimuth_times.size() - 2) {
                    time_index++;
                }

                // Direct access with '[]' is faster around half a second per GRD dataset.
                values[y - tile_offset_y][x - tile_offset_x] =
                    interpolated_azimuth_vector[y - ny0] *
                    InterpolateNoiseByTime(noise_range_vector_azimuth_times[time_index],
                                           noise_range_vector_azimuth_times[time_index + 1],
                                           interpolated_range_vectors[time_index][xx],
                                           interpolated_range_vectors[time_index + 1][xx], azimuth_time);
            }
        }
    }
}

}  // namespace alus::tnr
