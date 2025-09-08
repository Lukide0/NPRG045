#pragma once

#include "core/git/diff.h"
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>

namespace core::patch {

class PatchSplitter {
public:
    enum FileState {
        UNKNOWN       = 0,
        ALL_SELECTED  = 0b01,
        NONE_SELECTED = 0b10,
        SOME          = ALL_SELECTED | NONE_SELECTED,
    };

    PatchSplitter() = default;

    bool file_begin(const git::diff_files_header_t& header, FileState state = FileState::UNKNOWN);
    void process(const git::diff_line_t& line, const git::diff_hunk_t& hunk, bool selected);
    void file_end();

    bool is_whole_patch() const {
        return m_patch_selection == FileState::ALL_SELECTED || m_patch_selection == FileState::NONE_SELECTED;
    }

    bool is_whole_file() const {
        return m_file_selection == FileState::ALL_SELECTED || m_file_selection == FileState::NONE_SELECTED;
    }

    [[nodiscard]] std::string get_patch() const { return m_patch.str(); }

private:
    std::stringstream m_patch;
    std::stringstream m_file_header;
    std::stringstream m_file_patch;
    std::stringstream m_hunk_stream;

    const git::diff_files_header_t* m_header = nullptr;

    int m_lines_removed = 0;
    int m_lines_added   = 0;
    int m_lines_count   = 0;
    int m_hunk_offset   = 0;

    FileState m_file_selection  = FileState::UNKNOWN;
    FileState m_patch_selection = FileState::UNKNOWN;

    bool m_used = false;

    const git::diff_hunk_t* m_hunk = nullptr;

    void create_hunk();
    void prepare_header();
};

}
