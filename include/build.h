#pragma once

#include <string_view>

namespace build {

#ifdef SOURCE_ROOT
constexpr std::string_view source_directory = SOURCE_ROOT;
#else
constexpr std::string_view source_directory;
#endif

/**
 * @brief Removes the source directory prefix from a path if present.
 *
 * @param path Input path.
 *
 * @return Path without the source directory prefix if it starts with it,
 *         otherwise the original path.
 */
constexpr std::string_view remove_source_dir(std::string_view path) {
    if (!path.starts_with(source_directory)) {
        return path;
    }

    return path.substr(source_directory.size());
}

}
