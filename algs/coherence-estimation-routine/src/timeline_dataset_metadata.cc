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

#include "timeline_dataset_metadata.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <pugixml.hpp>

#include "zipper/unzipper.h"

namespace {

constexpr std::string_view MANIFEST_FILENAME{"manifest.safe"};
constexpr size_t MAX_RELATIVE_ORBIT{175U};

std::string_view GetLocalName(const pugi::xml_node& node) {
    std::string_view name{node.name()};
    if (const auto namespace_separator = name.rfind(':'); namespace_separator != std::string_view::npos) {
        name.remove_prefix(namespace_separator + 1U);
    }
    return name;
}

bool IsRelativeOrbitStartNode(const pugi::xml_node& node) {
    return GetLocalName(node) == "relativeOrbitNumber" &&
           std::string_view{node.attribute("type").value()} == "start";
}

bool IsOrbitDirectionNode(const pugi::xml_node& node) {
    return GetLocalName(node) == "pass";
}

bool IsManifestEntry(const zipper::ZipEntry& entry) {
    const auto filename = std::filesystem::path(entry.name).filename().string();
    return filename.size() == MANIFEST_FILENAME.size() &&
           std::equal(filename.cbegin(), filename.cend(), MANIFEST_FILENAME.cbegin(), [](char lhs, char rhs) {
               return std::tolower(static_cast<unsigned char>(lhs)) ==
                      std::tolower(static_cast<unsigned char>(rhs));
           });
}

}  // namespace

namespace alus::coherenceestimationroutine {

TimelineDatasetMetadata GetTimelineDatasetMetadataFromManifest(std::istream& manifest) {
    pugi::xml_document document;
    if (!document.load(manifest)) {
        return {};
    }

    TimelineDatasetMetadata metadata;
    const auto relative_orbit_node = document.find_node(IsRelativeOrbitStartNode);
    const auto relative_orbit = relative_orbit_node.text().as_uint();
    if (relative_orbit != 0U && relative_orbit <= MAX_RELATIVE_ORBIT) {
        metadata.relative_orbit = relative_orbit;
    }

    const auto orbit_direction_node = document.find_node(IsOrbitDirectionNode);
    std::string orbit_direction = orbit_direction_node.text().as_string();
    std::transform(orbit_direction.cbegin(), orbit_direction.cend(), orbit_direction.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    if (orbit_direction == "ascending" || orbit_direction == "descending") {
        metadata.orbit_direction = std::move(orbit_direction);
    }
    return metadata;
}

TimelineDatasetMetadata GetTimelineDatasetMetadata(const std::filesystem::path& dataset_path) {
    try {
        if (std::filesystem::is_directory(dataset_path)) {
            std::ifstream manifest{dataset_path / MANIFEST_FILENAME};
            return manifest.good() ? GetTimelineDatasetMetadataFromManifest(manifest) : TimelineDatasetMetadata{};
        }

        zipper::Unzipper archive{dataset_path.string()};
        const auto entries = archive.entries();
        const auto manifest_entry = std::find_if(entries.cbegin(), entries.cend(), IsManifestEntry);
        if (manifest_entry == entries.cend()) {
            return {};
        }

        std::stringstream manifest;
        if (!archive.extractEntryToStream(manifest_entry->name, manifest)) {
            return {};
        }
        manifest.seekg(0);
        return GetTimelineDatasetMetadataFromManifest(manifest);
    } catch (const std::exception&) {
        return {};
    }
}

}  // namespace alus::coherenceestimationroutine
