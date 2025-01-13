#include "utils.h"
#include <cstring>
#include <git2.h>
#include <git2/apply.h>
#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/errors.h>
#include <git2/global.h>
#include <git2/index.h>
#include <git2/object.h>
#include <git2/oid.h>
#include <git2/refs.h>
#include <git2/repository.h>
#include <git2/revert.h>
#include <git2/revparse.h>
#include <git2/signature.h>
#include <git2/tree.h>
#include <git2/types.h>
#include <iostream>
#include <span>

using args_t = std::span<const char* const>;

int revert(args_t args);
int reword(args_t args);
int squash(args_t args);
int fixup(args_t args);

void show_last_err() {
    const auto* const err_obj = git_error_last();
    std::cerr << "ERROR: " << err_obj->message << std::endl;
}

git_repository* g_repo = nullptr;

int main(int argc, char** argv) {

    if (argc < 2) {
        std::cerr << "ERROR: Missing arguments" << std::endl;
        return 1;
    }

    args_t args(argv + 1, argv + argc);

    const auto* const cmd = args[0];

    git_libgit2_init();

    int err = git_repository_open(&g_repo, ".");
    if (err != 0) {
        show_last_err();
        return 1;
    }

    int res = 0;

    if (std::strcmp(cmd, "revert") == 0) {
        res = revert(args.subspan(1));
    } else if (std::strcmp(cmd, "reword") == 0) {
        res = reword(args.subspan(1));
    } else if (std::strcmp(cmd, "squash") == 0) {
        res = squash(args.subspan(1));
    } else if (std::strcmp(cmd, "fixup") == 0) {
        res = fixup(args.subspan(1));

    } else {
        std::cerr << "ERROR: Unknown command: " << cmd << std::endl;
        res = 1;
    }

    git_repository_free(g_repo);
    git_libgit2_shutdown();

    return res;
}

int revert(args_t args) {
    if (args.size() > 1) {
        std::cout << "ERROR: Too many arguments" << std::endl;
        return 1;
    } else if (args.empty()) {
        std::cout << "ERROR: Missing commit" << std::endl;
        return 1;
    }

    const auto* const commit_hash = args[0];

    git_commit_t commit;
    if (!get_commit_from_hash(commit, commit_hash, g_repo)) {
        show_last_err();
        return 1;
    }
    git_commit_t last_commit;
    if (!get_last_commit(last_commit, g_repo)) {
        show_last_err();
        return 1;
    }

    git_index_t index;

    int err = git_revert_commit(&index.index, g_repo, commit.commit, last_commit.commit, 0, nullptr);
    if (err != 0) {
        show_last_err();
        return 1;
    }

    git_oid tree_id;
    err = git_index_write_tree_to(&tree_id, index.index, g_repo);
    if (err != 0) {
        show_last_err();
        return 1;
    }

    git_tree_t tree;
    err = git_tree_lookup(&tree.tree, g_repo, &tree_id);
    if (err != 0) {
        show_last_err();
        return 1;
    }

    git_signature_t me;
    err = git_signature_now(&me.sig, "Me", "me@example.com");
    if (err != 0) {
        show_last_err();
        return 1;
    }

    git_oid new_commit;
    err = git_commit_create_v(
        &new_commit, g_repo, "HEAD", me.sig, me.sig, "UTF-8", "Revert commit", tree.tree, 1, commit.commit
    );
    if (err != 0) {
        show_last_err();
        return 1;
    }

    return 0;
}

int reword(args_t args) {
    if (args.size() != 1 || strlen(args[0]) == 0) {
        std::cerr << "ERROR: Expected message argument" << std::endl;
        return 1;
    }

    git_commit_t commit;

    if (!get_last_commit(commit, g_repo)) {
        show_last_err();
        return 1;
    }

    git_reference_obj head_ref;
    if (git_repository_head(&head_ref.ref, g_repo) != 0) {
        show_last_err();
        return 1;
    }

    const git_oid* tree_id = git_commit_tree_id(commit.commit);

    git_tree_t tree;
    int err = git_tree_lookup(&tree.tree, g_repo, tree_id);
    if (err != 0) {
        show_last_err();
        return 1;
    }

    git_commit_parents_t parents;

    if (!get_all_parents(parents, commit.commit)) {
        show_last_err();
        return 1;
    }

    git_signature_t me;
    err = git_signature_now(&me.sig, "Me", "me@example.com");
    if (err != 0) {
        show_last_err();
        return 1;
    }

    git_oid new_commit;
    if (git_commit_create(
            &new_commit, g_repo, nullptr, me.sig, me.sig, "UTF-8", args[0], tree.tree, parents.count, parents.get()
        )
        != 0) {
        show_last_err();
        return 1;
    }

    git_reference_obj ref;
    git_reference_set_target(&ref.ref, head_ref.ref, &new_commit, "Reword commit");

    return 0;
}

int combine_commits(git_commit_t& commit, git_commit_t& parent_commit, const char* msg) {
    git_tree_t tree;
    git_tree_t parent_tree;

    git_commit_parents_t parents;
    if (!get_all_parents(parents, parent_commit.commit)) {
        show_last_err();
        return 1;
    }

    if (git_commit_tree(&tree.tree, commit.commit) != 0
        || git_commit_tree(&parent_tree.tree, parent_commit.commit) != 0) {
        show_last_err();
        return 1;
    }

    git_reference_obj head_ref;
    if (git_repository_head(&head_ref.ref, g_repo) != 0) {
        show_last_err();
        return 1;
    }

    git_diff_t diff;
    if (git_diff_tree_to_tree(&diff.diff, g_repo, parent_tree.tree, tree.tree, nullptr) != 0) {
        show_last_err();
        return 1;
    }

    git_index_t index;
    if (git_apply_to_tree(&index.index, g_repo, parent_tree.tree, diff.diff, nullptr) != 0) {
        show_last_err();
        return 1;
    }

    git_oid new_tree_id;
    if (git_index_write_tree_to(&new_tree_id, index.index, g_repo) != 0) {
        show_last_err();
        return 1;
    }

    git_tree_t new_tree;
    if (git_tree_lookup(&new_tree.tree, g_repo, &new_tree_id) != 0) {
        show_last_err();
        return 1;
    }

    git_signature_t me;
    if (git_signature_now(&me.sig, "Me", "me@example.com") != 0) {
        show_last_err();
        return 1;
    }

    git_oid new_commit;
    if (git_commit_create(
            &new_commit, g_repo, nullptr, me.sig, me.sig, "UTF-8", msg, new_tree.tree, parents.count, parents.get()
        )
        != 0) {
        show_last_err();
        return 1;
    }

    git_reference_obj ref;
    git_reference_set_target(&ref.ref, head_ref.ref, &new_commit, "Squash commits");

    return 0;
}

int combine_commits(args_t args, bool use_parent_msg) {
    if (!use_parent_msg) {
        if (args.size() != 2) {
            std::cerr << "ERROR: Expected only commit and message" << std::endl;
            return 1;
        }
    } else {
        if (args.size() != 1) {
            std::cerr << "ERROR: Expected only commit" << std::endl;
            return 1;
        }
    }

    const auto* const commit_hash = args[0];

    git_commit_t commit;
    if (!get_commit_from_hash(commit, commit_hash, g_repo)) {
        show_last_err();
        return 1;
    }

    const auto parent_count = git_commit_parentcount(commit.commit);

    if (parent_count > 1) {
        std::cerr << "ERROR: Commit has more than one parent" << std::endl;
        return 1;
    } else if (parent_count == 0) {
        std::cerr << "ERROR: Commit has no parents" << std::endl;
        return 1;
    }

    git_commit_t parent_commit;
    if (git_commit_parent(&parent_commit.commit, commit.commit, 0) != 0) {
        show_last_err();
        return 1;
    }

    const char* msg = (use_parent_msg) ? git_commit_message(parent_commit.commit) : args[1];

    return combine_commits(commit, parent_commit, msg);
}

int squash(args_t args) { return combine_commits(args, false); }

int fixup(args_t args) { return combine_commits(args, true); }
