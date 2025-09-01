#include "core/patch/PatchSplitter.h"
#include "core/git/diff.h"
#include "core/git/types.h"
#include "core/utils/todo.h"
#include <iostream>

#include <QMessageBox>

namespace core::patch {

using git::diff_files_header_t;
using git::diff_files_t;
using git::diff_hunk_t;
using git::diff_line_t;

bool PatchSplitter::file_begin(const diff_files_header_t& header) {

    using State = diff_files_t::State;

    m_header = &header;

    m_not_used       = true;
    m_file_selection = FileState::UNKNOWN;

    m_file_header.str("");
    m_file_header.clear();

    switch (header.state) {
    case State::ADDED:
    case State::MODIFIED:
    case State::DELETED:
    case State::RENAMED:
    case State::COPIED:
        return true;

    case State::UNMODIFIED:
    case State::IGNORED:
    case State::UNTRACKED:
    case State::TYPECHANGE:
    case State::UNREADABLE:
    case State::CONFLICTED:
        QMessageBox::critical(
            nullptr,
            "Unsupported Diff State",
            QString("File diff state '%1' is not supported.").arg(diff_files_t::state_to_str(header.state))
        );
        return false;
    }

    return true;
}

void PatchSplitter::prepare_header() {
    using State = diff_files_t::State;

    auto old_file = "a/" + m_header->old_file.path;
    auto new_file = "b/" + m_header->new_file.path;

    m_file_header << "diff --git " << old_file << ' ' << new_file << '\n';

    switch (m_header->state) {
    case State::ADDED:
        m_file_header << "new file mode 100644\n";
        m_file_header << "index " << git::format_oid(&m_header->old_file.id).data() << ".."
                      << git::format_oid(&m_header->new_file.id).data() << '\n';

        m_file_header << "--- /dev/null\n";
        m_file_header << "+++ " << new_file << '\n';
        break;

    case State::MODIFIED:
        m_file_header << "index " << git::format_oid(&m_header->old_file.id).data() << ".."
                      << git::format_oid(&m_header->new_file.id).data() << " 100644\n";

        m_file_header << "--- " << old_file << '\n';
        m_file_header << "+++ " << new_file << '\n';
        break;

    case State::DELETED:
        m_file_header << "deleted file mode 100644\n";
        m_file_header << "index " << git::format_oid(&m_header->old_file.id).data() << "..0000000\n";
        m_file_header << "--- " << old_file << '\n';
        m_file_header << "+++ /dev/null\n";
        break;

    case State::RENAMED:
        m_file_header << "similarity index " << m_header->similarity << "%\n";
        m_file_header << "rename from " << m_header->old_file.path << '\n';
        m_file_header << "rename to " << m_header->new_file.path << '\n';
        break;
    case State::COPIED:
        m_file_header << "similarity index " << m_header->similarity << "%\n";
        m_file_header << "copy from " << m_header->old_file.path << '\n';
        m_file_header << "copy to " << m_header->new_file.path << '\n';
        break;

    case State::UNMODIFIED:
    case State::IGNORED:
    case State::UNTRACKED:
    case State::TYPECHANGE:
    case State::UNREADABLE:
    case State::CONFLICTED:
        TODO("Not implemented");
    }
}

void PatchSplitter::file_end() {
    create_hunk();

    m_patch_selection = static_cast<FileState>(m_patch_selection | m_file_selection);

    if (!m_not_used) {
        prepare_header();
        m_patch << m_file_header.str();
        m_patch << m_file_patch.str();
    }

    m_file_patch.str("");
    m_file_patch.clear();

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
            m_file_selection = static_cast<FileState>(m_file_selection | FileState::ALL_SELECTED);
            m_not_used       = false;
        } else {
            m_lines_count -= 1;
            m_file_selection = static_cast<FileState>(m_file_selection | FileState::NONE_SELECTED);
        }
        break;
    case diff_line_t::Type::DELETION:
    case diff_line_t::Type::DELETION_NEWLINE:
        if (selected) {
            m_lines_removed += 1;
            m_hunk_stream << '-' << line.content << '\n';
            m_file_selection = static_cast<FileState>(m_file_selection | FileState::ALL_SELECTED);
            m_not_used       = false;
        } else {
            m_file_selection = static_cast<FileState>(m_file_selection | FileState::NONE_SELECTED);

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

    if (m_lines_added != 0 || m_lines_removed != 0) {
        m_file_patch << "@@ -" << old_offset << ',' << old_count << " +" << new_offset << ',' << new_count << " @@\n";
        m_file_patch << m_hunk_stream.str();
    }

    m_hunk_stream.str("");
    m_hunk_stream.clear();

    m_lines_removed = 0;
    m_lines_added   = 0;
}

}
