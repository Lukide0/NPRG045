#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

[[noreturn]] static void TODO(std::string_view msg = "", std::source_location loc = std::source_location()) {
    std::cerr << std::format("TODO[{}:{}]: {}", loc.file_name(), loc.line(), msg) << std::endl;
    assert(false);
}

enum class CmdType {
    INVALID,
    NONE,
    PICK,
    REWORD,
    EDIT,
    SQUASH,
    FIXUP,
    EXEC,
    BREAK,
    DROP,
    LABEL,
    RESET,
    MERGE,
    UPDATE_REF,
};

static constexpr const char* cmd_to_str(CmdType type) {
    switch (type) {
    case CmdType::INVALID:
        return "INVALID";
    case CmdType::NONE:
        return "NONE";
    case CmdType::BREAK:
        return "BREAK";
    case CmdType::EXEC:
        return "EXEC";
    case CmdType::PICK:
        return "PICK";
    case CmdType::REWORD:
        return "REWORD";
    case CmdType::EDIT:
        return "EDIT";
    case CmdType::SQUASH:
        return "SQUASH";
    case CmdType::FIXUP:
        return "FIXUP";
    case CmdType::DROP:
        return "DROP";
    case CmdType::LABEL:
        return "LABEL";
    case CmdType::RESET:
        return "RESET";
    case CmdType::MERGE:
        return "MERGE";
    case CmdType::UPDATE_REF:
        return "UPDATE_REF";
    }

    return "";
}

struct LineResult {
    CmdType type;
    std::string_view rest;
};

struct CommitAction {
    CmdType type;
    std::string hash;
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

struct ParseResult {
    std::vector<CommitAction> actions;
    std::string err;
};

inline ParseResult parse_file(const std::string& filepath) {
    auto file = std::ifstream(filepath);
    ParseResult res;
    if (!file.good()) {
        res.err = "Cannot open rebase todo file";
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
            res.err = "Invalid command in rebase todo file";
            return res;
        case CmdType::NONE:
        case CmdType::BREAK:
        case CmdType::EXEC:
            goto no_commit;
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
            TODO("Implement");
            break;
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
