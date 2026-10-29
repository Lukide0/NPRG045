#include "core/action/Converter.h"

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/commit.h"
#include "core/git/types.h"
#include "logging/Log.h"

#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/tree.h>
#include <git2/types.h>

#include <cassert>
#include <iomanip>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

namespace core::action {

struct ActionInfo {
    std::string oid;
    std::string msg;
    std::istringstream new_msg;
};

struct ConverterContext {
    git_repository* repo;

    git::tree_t root_tree;
    Action* last_action = nullptr;
    git::commit_t base_commit;
    git_oid base_tree_id { };
};

bool set_base(ConverterContext& ctx, const git_oid& commit_id, const git_oid& tree_id) {
    git::commit_t commit;
    if (git_commit_lookup(&commit, ctx.repo, &commit_id) != 0) {
        return false;
    }

    ctx.base_commit  = std::move(commit);
    ctx.base_tree_id = tree_id;
    return true;
}

// Returns a commit whose tree is 'tree'. The current base is reused when its tree already matches,
// otherwise a parentless helper commit is created (only its tree matters).
const git_commit* ensure_base_commit(ConverterContext& ctx, const git_tree* tree, const git_commit* like) {
    const git_oid* tree_id = git_tree_id(tree);

    if (ctx.base_commit.get() != nullptr && git_oid_equal(&ctx.base_tree_id, tree_id) != 0) {
        return ctx.base_commit.get();
    }

    git_oid oid;
    const git_signature* author    = git_commit_author(like);
    const git_signature* committer = git_commit_committer(like);

    if (!git::create_commit(&oid, ctx.repo, author, committer, "rebase base", tree, nullptr, 0)) {
        return nullptr;
    }

    if (!set_base(ctx, oid, *tree_id)) {
        return nullptr;
    }

    return ctx.base_commit.get();
}

bool create_resolution_commit(
    ConverterContext& ctx, const git_commit* commit, const git_tree* prev_tree, const git_tree* resolution, git_oid& out
) {
    const git_commit* parent = ensure_base_commit(ctx, prev_tree, commit);
    if (parent == nullptr) {
        return false;
    }

    const git_signature* author    = git_commit_author(commit);
    const git_signature* committer = git_commit_committer(commit);

    const char* message = git_commit_message(commit);

    if (!git::create_commit(&out, ctx.repo, author, committer, message != nullptr ? message : "", resolution, parent)) {
        return false;
    }

    return set_base(ctx, out, *git_tree_id(resolution));
}

std::optional<ActionInfo>
get_action_info(ConverterContext& ctx, Action& act, conflict::ConflictManager& conflict_manager) {
    ActionInfo info;

    git_commit* commit = act.get_commit();

    const char* summary = git_commit_summary(commit);
    info.msg            = summary != nullptr ? summary : "";
    info.oid            = git_oid_tostr_s(git_commit_id(commit));

    const git_tree* prev_tree = (ctx.last_action == nullptr) ? ctx.root_tree.get() : ctx.last_action->get_tree();

    if (prev_tree != nullptr && act.get_type() != ActionType::DROP) {
        git_tree* resolution = conflict_manager.get_trees_resolution(prev_tree, commit);

        if (resolution != nullptr) {
            git_oid oid;

            if (!create_resolution_commit(ctx, commit, prev_tree, resolution, oid)) {
                logging::Log::error("Failed to create commit for tree");
                return std::nullopt;
            }

            info.oid = git_oid_tostr_s(&oid);
        }
    }

    if (act.has_msg()) {
        const auto msg = *act.get_msg();

        if (msg.empty()) {
            logging::Log::error("Empty commit message");
            return std::nullopt;
        }

        info.new_msg.str(msg);
    }

    return info;
}

void pick_to_todo(std::ostream& output, ActionInfo& info) { output << "pick " << info.oid << ' ' << info.msg << '\n'; }

void drop_to_todo(std::ostream& output, ActionInfo& info) { output << "drop " << info.oid << ' ' << info.msg << '\n'; }

void edit_to_todo(std::ostream& output, ActionInfo& info) { output << "edit " << info.oid << ' ' << info.msg << '\n'; }

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
    output << "squash " << info.oid << ' ' << info.msg << '\n';
}

void fixup_to_todo(std::ostream& output, ActionInfo& info) {
    output << "fixup " << info.oid << ' ' << info.msg << '\n';
}

bool Converter::actions_to_todo(
    std::ostream& output, ActionsManager& manager, conflict::ConflictManager& conflict_manager, bool insert_break
) {
    ConverterContext ctx;
    git_commit* root = manager.get_root_commit();

    assert(root != nullptr);
    ctx.repo = git_commit_owner(root);

    if (git_commit_tree(&ctx.root_tree, root) != 0) {
        logging::Log::error("Failed to read the tree of the root commit");
        return false;
    }

    if (!set_base(ctx, *git_commit_id(root), *git_tree_id(ctx.root_tree.get()))) {
        logging::Log::error("Failed to read the root commit");
        return false;
    }

    if (insert_break) {
        output << "break\n";
    }

    for (auto& act : manager) {
        auto info = get_action_info(ctx, act, conflict_manager);

        if (!info.has_value()) {
            return false;
        }

        ctx.last_action = &act;

        switch (act.get_type()) {
        case ActionType::PICK:
            pick_to_todo(output, *info);
            break;

        case ActionType::DROP:
            drop_to_todo(output, *info);
            break;

        case ActionType::SQUASH:
            squash_to_todo(output, *info);
            break;

        case ActionType::FIXUP:
            fixup_to_todo(output, *info);
            break;

        case ActionType::REWORD:
            reword_to_todo(output, *info);
            break;

        case ActionType::EDIT:
            edit_to_todo(output, *info);
            break;
        }
    }

    return true;
}

}
