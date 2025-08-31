#include "core/git/parser.h"
#include "core/git/paths.h"
#include "core/utils/todo.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace core::git {

struct LineResult {
    CmdType type;
    std::string_view rest;
};

inline std::string_view skip_whitespace(std::string_view str) {
    std::uint32_t index = 0;
    for (; index < str.size() && (std::isspace(str[index]) != 0); ++index) { }

    index = std::min<std::size_t>(index, str.size() - 1);

    return str.substr(index);
}

inline std::string_view extract_word(std::string_view str, std::string& out) {
    str = skip_whitespace(str);

    std::uint32_t index = 0;
    for (; index < str.size() && (std::isspace(str[index]) == 0); ++index) { }

    index = std::min<std::size_t>(index, str.size() - 1);

    out = str.substr(0, index);

    return str.substr(index);
}

inline LineResult parse_line(std::string_view line) {
    using namespace std::string_view_literals;

    const auto full_match = [](std::string_view& str, std::string_view full_name, char short_name) -> bool {
        if (str.size() > 2 && str[0] == short_name && std::isspace(str[1])) {
            str = str.substr(2);
            return true;
        } else if (str.size() > full_name.size() && str.starts_with(full_name) && std::isspace(str[full_name.size()])) {
            str = str.substr(full_name.size() + 1);
            return true;
        } else {
            return false;
        }
    };

    skip_whitespace(line);

    // comment
    if (line.starts_with('#') || line.empty()) {
        return { .type = CmdType::NONE, .rest = line };
    } else if (full_match(line, "pick"sv, 'p')) {
        return { .type = CmdType::PICK, .rest = line };
    } else if (full_match(line, "reword"sv, 'r')) {
        return { .type = CmdType::REWORD, .rest = line };
    } else if (full_match(line, "edit"sv, 'e')) {
        return { .type = CmdType::EDIT, .rest = line };
    } else if (full_match(line, "squash"sv, 's')) {
        return { .type = CmdType::SQUASH, .rest = line };
    } else if (full_match(line, "fixup"sv, 'f')) {
        return { .type = CmdType::FIXUP, .rest = line };
    } else if (full_match(line, "exec"sv, 'x')) {
        return { .type = CmdType::EXEC, .rest = line };
    } else if (full_match(line, "break"sv, 'b')) {
        return { .type = CmdType::BREAK, .rest = line };
    } else if (full_match(line, "drop"sv, 'd')) {
        return { .type = CmdType::DROP, .rest = line };
    } else if (full_match(line, "label"sv, 'l')) {
        return { .type = CmdType::LABEL, .rest = line };
    } else if (full_match(line, "reset"sv, 't')) {
        return { .type = CmdType::RESET, .rest = line };
    } else if (full_match(line, "merge"sv, 'm')) {
        return { .type = CmdType::MERGE, .rest = line };
    } else if (full_match(line, "update-ref"sv, 'u')) {
        return { .type = CmdType::UPDATE_REF, .rest = line };
    } else {
        return { .type = CmdType::INVALID, .rest = line };
    }
}

ParseResult parse_file(const std::string& filepath) {
    auto file = std::ifstream(filepath);
    ParseResult res;
    if (!file.good()) {
        res.err = "Cannot read rebase TODO file. Make sure you're in the middle of a git rebase.";
        return res;
    }

    // TODO: Return information about the rename (-C, -c)

    std::string line;
    while (std::getline(file, line)) {
        auto line_res = parse_line(line);

        auto args_raw = line_res.rest;
        std::string commit_hash;

        switch (line_res.type) {
        case CmdType::INVALID:
            res.err = "Unknown command found in git rebase TODO file. The file may be corrupted or use unsupported "
                      "rebase actions.";
            return res;
        case CmdType::NONE:
        case CmdType::BREAK:
            goto no_commit;
        case CmdType::EXEC:
            res.err = "Execution commands (exec) are not supported. Please remove or edit this command manually.";
            return res;
        case CmdType::PICK:
        case CmdType::REWORD:
        case CmdType::EDIT:
        case CmdType::SQUASH:
        case CmdType::DROP:
            args_raw = extract_word(args_raw, commit_hash);
            break;
        case CmdType::FIXUP:
            args_raw = skip_whitespace(args_raw);
            if (args_raw.starts_with("-C") || args_raw.starts_with("-c")) {
                args_raw = args_raw.substr(2);
            }

            args_raw = extract_word(args_raw, commit_hash);
            break;
        case CmdType::LABEL:
        case CmdType::RESET:
            // NOTE: The commit hash is used as the label name
            args_raw = extract_word(args_raw, commit_hash);
            break;
        case CmdType::MERGE:
            if (args_raw.starts_with("-C") || args_raw.starts_with("-c")) {
                args_raw = args_raw.substr(2);
                std::string tmp;
                args_raw = extract_word(args_raw, tmp);
            }

            // NOTE: The commit hash is used as the label name
            args_raw = extract_word(args_raw, commit_hash);
            break;

        case CmdType::UPDATE_REF:
            res.err
                = "Update ref commands (update-ref) are not supported. Please remove or edit this command manually.";
            return res;
        }

        if (commit_hash.empty()) {
            res.err = "Failed to extract commit hash";
            return res;
        }

        res.actions.push_back({ .type = line_res.type, .hash = commit_hash });

    no_commit:;
    }

    return res;
}

std::optional<const char*> get_rebase_info(const std::string& repo, std::string& out_head, std::string& out_onto) {

    {
        auto head_file = std::ifstream(repo + '/' + HEAD_FILE.c_str());
        auto onto_file = std::ifstream(repo + '/' + ONTO_FILE.c_str());

        if (!head_file.good() || !onto_file) {
            return "Cannot find git rebase files. Make sure you're in an active rebase operation.";
        }
        std::getline(head_file, out_head);
        std::getline(onto_file, out_onto);
    }

    if (out_head.empty() || out_onto.empty()) {
        return "Git rebase files are empty or corrupted. The rebase operation may be in an invalid state.";
    }

    return std::nullopt;
}

}
