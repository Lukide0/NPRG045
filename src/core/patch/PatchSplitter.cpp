#include "core/patch/PatchSplitter.h"
#include "core/git/diff.h"
#include "core/git/types.h"
#include <iostream>
#include <string>

namespace core::patch {

using git::diff_files_header_t;
using git::diff_hunk_t;
using git::diff_line_t;

void PatchSplitter::file_begin(const diff_files_header_t& header) {

    auto old_file = "a/" + header.old_file.path;
    auto new_file = "b/" + header.new_file.path;

    m_patch << "diff --git " << old_file << ' ' << new_file << '\n';
    m_patch << "index " << git::format_oid(&header.old_file.id).data() << ".."
            << git::format_oid(&header.new_file.id).data() << " 100644\n";

    m_patch << "--- " << old_file << '\n';
    m_patch << "+++ " << new_file << '\n';
}

void PatchSplitter::file_end() {
    create_hunk();
    m_hunk = nullptr;
}

void PatchSplitter::process(const diff_line_t& line, const diff_hunk_t& hunk, bool selected) {

    // 1st hunk
    if (m_hunk == nullptr) {
        m_hunk = &hunk;
    }
    // new hunk
    else if (m_hunk != &hunk) {
        create_hunk();
        m_hunk = &hunk;
    }

    m_lines_count += 1;

    switch (line.type) {
    case diff_line_t::Type::CONTEXT:
        m_hunk_stream << ' ' << line.content << '\n';
        break;

    case diff_line_t::Type::CONTEXT_NO_NEWLINE:
        m_hunk_stream << "\\ No newline at end of file\n";
        break;

    case diff_line_t::Type::ADDITION:
    case diff_line_t::Type::ADDITION_NEWLINE:
        if (selected) {
            m_lines_added += 1;
            m_hunk_stream << '+' << line.content << '\n';
            m_empty = false;
        } else {
            m_lines_count -= 1;
            m_all = false;
        }
        break;
    case diff_line_t::Type::DELETION:
    case diff_line_t::Type::DELETION_NEWLINE:
        if (selected) {
            m_lines_removed += 1;
            m_hunk_stream << '-' << line.content << '\n';
            m_empty = false;
        } else {
            m_all = false;
            m_hunk_stream << ' ' << line.content << '\n';
        }
        break;
    }
}

void PatchSplitter::create_hunk() {
    const int old_offset = m_hunk->old_file.offset;
    const int old_count  = m_hunk->old_file.count;

    int new_offset = old_offset + m_hunk_offset;
    int new_count  = old_count + (m_lines_added - m_lines_removed);

    m_hunk_offset += m_lines_added - m_lines_removed;

    m_patch << "@@ -" << old_offset << ',' << old_count << " +" << new_offset << ',' << new_count << " @@\n";
    m_patch << m_hunk_stream.str();

    m_hunk_stream.clear();
    m_lines_removed = 0;
    m_lines_added   = 0;
}

}
