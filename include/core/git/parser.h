#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <vector>

namespace core::git {

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

struct CommitAction {
    CmdType type;
    std::string hash;
};

struct ParseResult {
    std::vector<CommitAction> actions;
    std::string err;
};

ParseResult parse_file(const std::string& filepath);

std::optional<const char*> get_rebase_info(const std::string& repo, std::string& out_head, std::string& out_onto);

}
