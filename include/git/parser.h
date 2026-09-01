#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace git {

/**
 * @brief Type of rebase command.
 */
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

/**
 * @brief Converts a CmdType to its string representation.
 */
static constexpr const char* cmd_to_str(CmdType type) {
    switch (type) {
    case CmdType::INVALID:
        return "invalid";
    case CmdType::NONE:
        return "none";
    case CmdType::BREAK:
        return "break";
    case CmdType::EXEC:
        return "exec";
    case CmdType::PICK:
        return "pick";
    case CmdType::REWORD:
        return "reword";
    case CmdType::EDIT:
        return "edit";
    case CmdType::SQUASH:
        return "squash";
    case CmdType::FIXUP:
        return "fixup";
    case CmdType::DROP:
        return "drop";
    case CmdType::LABEL:
        return "label";
    case CmdType::RESET:
        return "reset";
    case CmdType::MERGE:
        return "merge";
    case CmdType::UPDATE_REF:
        return "update_ref";
    }

    return "";
}

/**
 * @brief Represents a parsed rebase command action.
 */
struct CommitAction {
    CmdType type;
    std::string hash;
    std::size_t line;
};

/**
 * @brief Result of parsing a rebase file.
 */
struct ParseResult {
    std::vector<CommitAction> actions;
    std::string err;
};

/**
 * @brief Parses a rebase todo file.
 *
 * @param filepath Path to the file to parse.
 *
 * @return Parsed actions and possible error message.
 */
ParseResult parse_file(const std::string& filepath);

struct RebaseInfoError {
    enum Type {
        NotFound,
        EmptyOrCurrupted,
    } type;

    const char* msg;

    constexpr RebaseInfoError(Type t, const char* m)
        : type(t)
        , msg(m) { }
};

/**
 * @brief Retrieves rebase information from a repository.
 *
 * @param repo Repository path.
 * @param out_head Output HEAD reference.
 * @param out_onto Output ONTO reference.
 *
 * @return Optional error message (nullopt if successful).
 */
std::optional<RebaseInfoError> get_rebase_info(const std::string& repo, std::string& out_head, std::string& out_onto);

}
