#include "action/Converter.h"
#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/git/commit.h"
#include "logging/Log.h"

#include <git2/commit.h>
#include <git2/oid.h>

#include <array>
#include <cassert>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

namespace action {

struct ActionInfo {
    std::array<char, 256> buff;
    std::string oid;
    std::string msg;
    std::istringstream new_msg;
};

struct ConverterContext {
    git_commit* root;
    git_commit* last_commit;
    git_repository* repo;
    Action* last_action = nullptr;
};

std::pair<ActionInfo, bool> get_action_info(
    ConverterContext& ctx,
    Action& act,
    action::ActionsManager& manager,
    core::conflict::ConflictManager& conflict_manager
) {
    using namespace core;

    ActionInfo info;

    git_commit* parent_commit = ctx.last_commit;
    git_commit* commit        = act.get_commit();

    git_tree* resolution_tree = nullptr;
    if (ctx.last_action != nullptr && ctx.last_action->get_tree() != nullptr) {

        resolution_tree = conflict_manager.get_trees_resolution(ctx.last_action->get_tree(), commit);

    } else {

        git::tree_t root_tree;
        assert(ctx.last_commit == ctx.root);

        if (git_commit_tree(&root_tree, ctx.last_commit) != 0) {
            return std::make_pair<ActionInfo, bool>(std::move(info), false);
        }

        resolution_tree = conflict_manager.get_trees_resolution(root_tree, commit);
    }

    info.msg = git_commit_summary(commit);

    // conflict resolution
    if (resolution_tree != nullptr) {

        git_oid oid;
        const auto* sig_author    = git_commit_author(commit);
        const auto* sig_committer = git_commit_committer(commit);

        if (!git::create_commit(
                &oid, ctx.repo, sig_author, sig_committer, info.msg.c_str(), resolution_tree, parent_commit
            )) {
            logging::Log::error("Failed to create commit for tree");
            // NOTE: The oid is nullptr
            return std::make_pair<ActionInfo, bool>(std::move(info), false);
        }

        info.oid = git_oid_tostr_s(&oid);
    } else {
        info.oid = git_oid_tostr_s(git_commit_id(commit));
    }

    if (act.has_msg()) {
        auto msg_id      = act.get_msg_id().value();
        std::string& msg = manager.get_msg(msg_id);

        if (msg.empty()) {
            return std::make_pair<ActionInfo, bool>(std::move(info), false);
        }

        info.new_msg.str(msg);
    }

    return std::make_pair<ActionInfo, bool>(std::move(info), true);
}

void pick_to_todo(std::ostream& output, ActionInfo& info) { output << "pick " << info.oid << ' ' << info.msg << '\n'; }

void drop_to_todo(std::ostream& output, ActionInfo& info) { output << "drop " << info.oid << ' ' << info.msg << '\n'; }

void edit_to_todo(std::ostream& output, ActionInfo& info) { output << "drop " << info.oid << ' ' << info.msg << '\n'; }

void reword_to_todo(std::ostream& output, ActionInfo& info) {
    // 1. pick the commit
    output << "pick " << info.oid << ' ' << info.msg << '\n';

    // 2. change the commit message
    output << "exec " << "git commit --amend --only";

    for (std::string line; std::getline(info.new_msg, line);) {
        output << " -m " << std::quoted(line);
    }

    output << '\n';
}

void squash_to_todo(std::ostream& output, ActionInfo& info) {
    output << "fixup " << info.oid << ' ' << info.msg << '\n';
}

void fixup_to_todo(std::ostream& output, ActionInfo& info) {
    output << "fixup " << info.oid << ' ' << info.msg << '\n';
}

bool Converter::actions_to_todo(
    std::ostream& output, ActionsManager& manager, core::conflict::ConflictManager& conflict_manager
) {
    ConverterContext ctx;
    ctx.root = manager.get_root_commit();

    assert(ctx.root != nullptr);

    ctx.last_commit = ctx.root;
    ctx.repo        = git_commit_owner(ctx.root);

    for (auto& act : manager) {
        auto&& [info, status] = get_action_info(ctx, act, manager, conflict_manager);

        ctx.last_action = &act;

        if (!status) {
            return false;
        }

        switch (act.get_type()) {
        case ActionType::PICK:
            pick_to_todo(output, info);
            break;

        case ActionType::DROP:
            drop_to_todo(output, info);
            break;

        case ActionType::SQUASH:
            squash_to_todo(output, info);
            break;

        case ActionType::FIXUP:
            fixup_to_todo(output, info);
            break;

        case ActionType::REWORD:
            reword_to_todo(output, info);
            break;

        case ActionType::EDIT:
            edit_to_todo(output, info);
            break;
        }
    }

    return true;
}

}
