#include "git/parser.h"

#include "git/paths.h"

#include <array>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace git {

struct LineResult {
    CmdType type;
    std::string_view rest;
};

struct CmdSpec {
    std::string_view full;
    char shorthand;
    CmdType type;

    [[nodiscard]] bool match(std::string_view value) const {
        return (shorthand != '\0' && value.size() == 1 && value.front() == shorthand) || value == full;
    }
};

inline constexpr auto COMMANDS = std::to_array<CmdSpec>({
    { .full = "pick",       .shorthand = 'p',  .type = CmdType::PICK       },
    { .full = "reword",     .shorthand = 'r',  .type = CmdType::REWORD     },
    { .full = "edit",       .shorthand = 'e',  .type = CmdType::EDIT       },
    { .full = "squash",     .shorthand = 's',  .type = CmdType::SQUASH     },
    { .full = "fixup",      .shorthand = 'f',  .type = CmdType::FIXUP      },
    { .full = "exec",       .shorthand = 'x',  .type = CmdType::EXEC       },
    { .full = "break",      .shorthand = 'b',  .type = CmdType::BREAK      },
    { .full = "drop",       .shorthand = 'd',  .type = CmdType::DROP       },
    { .full = "label",      .shorthand = 'l',  .type = CmdType::LABEL      },
    { .full = "reset",      .shorthand = 't',  .type = CmdType::RESET      },
    { .full = "merge",      .shorthand = 'm',  .type = CmdType::MERGE      },
    { .full = "update-ref", .shorthand = 'u',  .type = CmdType::UPDATE_REF },
    { .full = "noop",       .shorthand = '\0', .type = CmdType::NONE       },
});

bool is_space(char c) { return std::isspace(static_cast<int>(c)) != 0; }

std::string_view skip_whitespace(std::string_view str) {
    std::size_t i = 0;
    while (i < str.size() && is_space(str[i])) {
        i += 1;
    }
    return str.substr(i);
}

std::string_view extract_word(std::string_view str, std::string& out) {
    str = skip_whitespace(str);

    std::size_t i = 0;
    while (i < str.size() && !is_space(str[i])) {
        ++i;
    }

    out.assign(str.substr(0, i));
    return str.substr(i);
}

bool extract_commit_flag(std::string_view& args, std::string& flag) {
    const auto after = extract_word(args, flag);
    if (flag == "-C" || flag == "-c") {
        args = after;
        return true;
    }
    return false;
}

LineResult parse_line(std::string_view line, std::string_view comment_char) {
    line = skip_whitespace(line);

    if (line.empty() || line.starts_with(comment_char)) {
        return { .type = CmdType::NONE, .rest = line };
    }

    std::size_t n = 0;
    while (n < line.size() && !is_space(line[n])) {
        n += 1;
    }

    const std::string_view word = line.substr(0, n);
    const std::string_view rest = skip_whitespace(line.substr(n));

    for (const auto& spec : COMMANDS) {
        if (spec.match(word)) {
            return { .type = spec.type, .rest = rest };
        }
    }

    return { .type = CmdType::INVALID, .rest = line };
}

ParseResult parse_stream(std::istream& stream, std::string_view comment_char) {
    ParseResult res;

    std::size_t line_num = 0;
    std::string line;
    while (std::getline(stream, line)) {
        line_num += 1;

        const auto line_res   = parse_line(line, comment_char);
        std::string_view args = line_res.rest;

        std::string hash;

        // TODO:
        std::string flags;

        const auto fail = [&](const char* msg) {
            res.err = msg;
            return res;
        };

        switch (line_res.type) {
        case CmdType::INVALID:
            return fail(
                "Unknown command found in git rebase TODO file. The file may be corrupted or use unsupported "
                "rebase actions."
            );

        case CmdType::NONE:
            continue;

        case CmdType::BREAK:
            return fail("Break commands (break) are not supported. Please remove or edit this command manually.");

        case CmdType::EXEC:
            return fail("Execution commands (exec) are not supported. Please remove or edit this command manually.");

        case CmdType::UPDATE_REF:
            return fail(
                "Update ref commands (update-ref) are not supported. Please remove or edit this command "
                "manually."
            );

        case CmdType::LABEL:
            return fail("Label commands (label) are not supported. Please remove or edit this command manually.");

        case CmdType::RESET:
            return fail("Reset commands (reset) are not supported. Please remove or edit this command manually.");

        case CmdType::MERGE:
            return fail("Merge commands (merge) are not supported. Please remove or edit this command manually.");

        case CmdType::PICK:
        case CmdType::REWORD:
        case CmdType::EDIT:
        case CmdType::SQUASH:
        case CmdType::DROP:
            extract_word(args, hash);
            break;

        case CmdType::FIXUP:
            // `fixup [-C | -c] <commit>`
            extract_commit_flag(args, flags);
            extract_word(args, hash);
            break;
        }

        if (hash.empty()) {
            return fail("Failed to extract commit hash");
        }

        res.actions.push_back({ .type = line_res.type, .hash = std::move(hash), .line = line_num });
    }

    return res;
}

ParseResult parse_file(const std::string& filepath, std::string_view comment_char) {
    std::ifstream file(filepath);
    if (!file.good()) {
        ParseResult res;
        res.err = "Cannot read rebase TODO file. Make sure you're in the middle of a git rebase.";
        return res;
    }
    return parse_stream(file, comment_char);
}

std::optional<RebaseInfoError> get_rebase_info(const std::string& repo, std::string& out_head, std::string& out_onto) {

    {
        auto head_file = std::ifstream(repo + '/' + HEAD_FILE.c_str());
        auto onto_file = std::ifstream(repo + '/' + ONTO_FILE.c_str());

        if (!head_file || !onto_file) {
            return std::make_optional<RebaseInfoError>(
                RebaseInfoError::NotFound,
                "Cannot find git rebase files. Make sure you're in an active rebase operation."
            );
        }

        std::getline(head_file, out_head);
        std::getline(onto_file, out_onto);
    }

    if (out_head.empty() || out_onto.empty()) {
        return std::make_optional<RebaseInfoError>(
            RebaseInfoError::EmptyOrCorrupted,
            "Git rebase files are empty or corrupted. The rebase operation may be in an invalid state."
        );
    }

    return std::nullopt;
}
}
