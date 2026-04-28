#pragma once

#include "core/git/diff.h"
#include <sstream>
#include <string>

namespace core::patch {

/**
 * @brief Splits a diff patch into selected and unselected parts.
 */
class PatchSplitter {
public:
    /**
     * @brief Represents selection state of a file or patch.
     */
    enum FileState {
        UNKNOWN       = 0,
        ALL_SELECTED  = 0b01,
        NONE_SELECTED = 0b10,
        SOME          = ALL_SELECTED | NONE_SELECTED,
    };

    PatchSplitter() = default;

    /**
     * @brief Begins processing a diff file.
     *
     * @param header Diff file header.
     * @param state Initial selection state.
     *
     * @return True if file can be processed.
     */
    bool file_begin(const git::diff_files_header_t& header, FileState state = FileState::UNKNOWN);

    /**
     * @brief Processes a diff line within a hunk.
     *
     * @param line Diff line.
     * @param hunk Parent hunk.
     * @param selected Whether the line is selected.
     */
    void process(const git::diff_line_t& line, const git::diff_hunk_t& hunk, bool selected);

    /**
     * @brief Ends processing of current file.
     */
    void file_end();

    /**
     * @brief Marks current file as not selected.
     */
    void not_selected_file() {
        m_patch_selection = static_cast<FileState>(m_patch_selection | FileState::NONE_SELECTED);
    }

    /**
     * @brief Checks if the patch is fully selected or fully unselected.
     */
    bool is_whole_patch() const {
        return m_patch_selection == FileState::ALL_SELECTED || m_patch_selection == FileState::NONE_SELECTED;
    }

    /**
     * @brief Checks if the current file is fully selected or unselected.
     */
    bool is_whole_file() const {
        return m_file_selection == FileState::ALL_SELECTED || m_file_selection == FileState::NONE_SELECTED;
    }

    /**
     * @brief Gets the resulting patch as a string.
     */
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
