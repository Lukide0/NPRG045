#pragma once

#include "core/git/diff.h"
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>

namespace core::patch {

class PatchSplitter {
public:
    PatchSplitter() = default;

    void file_begin(const git::diff_files_header_t& file);
    void process(const git::diff_line_t& line, const git::diff_hunk_t& hunk, bool selected);
    void file_end();

    bool is_whole_patch() const { return m_all || m_empty; }

    [[nodiscard]] std::string get_patch() const { return m_patch.str(); }

private:
    std::stringstream m_patch;
    std::stringstream m_file_patch;
    std::stringstream m_hunk_stream;

    int m_lines_removed = 0;
    int m_lines_added   = 0;
    int m_lines_count   = 0;
    int m_hunk_offset   = 0;


    bool m_not_used = true;
    bool m_all   = true;
    bool m_empty = true;

    const git::diff_hunk_t* m_hunk = nullptr;

    void create_hunk();
};

}
