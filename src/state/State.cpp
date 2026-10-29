#include "state/State.h"

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"
#include "state/AppState.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/repository.h>
#include <git2/tree.h>
#include <git2/types.h>

#include <QDomDocument>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <Qt>
#include <QTextStream>

namespace state {
using core::git::OID_SIZE;

constexpr const char* ROOT_NODE             = "save_data";
constexpr const char* ROOT_COMMIT           = "root";
constexpr const char* ACTIONS_NODE          = "actions";
constexpr const char* ACTION_NODE           = "action";
constexpr const char* CONFLICT_COMMITS_NODE = "conflict_commits";
constexpr const char* CONFLICT_COMMIT_NODE  = "conflict_commit";
constexpr const char* CONFLICTS_NODE        = "conflicts";
constexpr const char* CONFLICT_NODE         = "conflict";

std::string load_message(const QDomElement& action) {
    QDomNodeList nodes = action.elementsByTagName("line");

    QStringList lines;

    for (int i = 0; i < nodes.size(); ++i) {
        auto&& line = nodes.at(i);
        lines << line.toElement().text();
    }

    return lines.join('\n').toStdString();
}

void save_message(const QString& msg, QDomDocument& doc, QDomElement& action) {

    auto lines = msg.split(QRegularExpression("\n|\r\n"), Qt::KeepEmptyParts);

    for (auto&& line : lines) {
        auto line_el = doc.createElement("line");
        line_el.appendChild(doc.createTextNode(line));

        action.appendChild(line_el);
    }
}

void save_actions(const core::action::ActionsManager& manager, QDomElement& root, QDomDocument& doc) {

    auto* cmt = manager.get_root_commit();

    QDomElement root_commit = doc.createElement(ROOT_COMMIT);
    root_commit.setAttribute(
        "hash", QString::fromStdString(core::git::format_oid_to_str<OID_SIZE>(git_commit_id(cmt)))
    );
    root.appendChild(root_commit);

    QDomElement actions = doc.createElement(ACTIONS_NODE);

    for (auto&& act : manager) {
        QDomElement action = doc.createElement(ACTION_NODE);

        const git_oid& id = act.get_oid();
        action.setAttribute("hash", QString::fromStdString(core::git::format_oid_to_str<core::git::OID_SIZE>(&id)));

        switch (act.get_type()) {
        case core::action::ActionType::PICK:
            action.setAttribute("type", "pick");
            break;
        case core::action::ActionType::SQUASH:
            action.setAttribute("type", "squash");
            break;
        case core::action::ActionType::FIXUP:
            action.setAttribute("type", "fixup");
            break;
        case core::action::ActionType::REWORD:
            action.setAttribute("type", "reword");
            break;
        case core::action::ActionType::EDIT:
            action.setAttribute("type", "edit");
            break;
        case core::action::ActionType::DROP:
            action.setAttribute("type", "drop");
            break;
        }

        auto msg = act.get_msg();
        if (act.has_msg()) {
            save_message(QString::fromStdString(*msg), doc, action);
        }

        actions.appendChild(action);
    }

    root.appendChild(actions);
}

bool load_actions(QDomElement& root, SaveData& save_data, git_repository* repo) {
    // -- Root commit ---------------------------------------------------------
    QDomElement root_commit = root.firstChildElement(ROOT_COMMIT);
    if (root_commit.isNull() || !root_commit.hasAttribute("hash") || root_commit.attribute("hash").isEmpty()) {
        return false;
    }

    auto root_commit_hash = root_commit.attribute("hash").toStdString();

    if (!core::git::get_commit_from_hash(save_data.root, root_commit_hash.c_str(), repo)) {
        return false;
    }

    // -- Actions -------------------------------------------------------------
    QDomElement actions = root.firstChildElement(ACTIONS_NODE);
    if (actions.isNull()) {
        return false;
    }

    for (QDomNode node = actions.firstChildElement(ACTION_NODE); !node.isNull();
         node          = node.nextSiblingElement(ACTION_NODE)) {

        // -- Action ----------------------------------------------------------
        QDomElement el = node.toElement();

        QString type_str = el.attribute("type");
        core::action::ActionType type;

        if (type_str == "pick") {
            type = core::action::ActionType::PICK;
        } else if (type_str == "edit") {
            type = core::action::ActionType::EDIT;
        } else if (type_str == "drop") {
            type = core::action::ActionType::DROP;
        } else if (type_str == "fixup") {
            type = core::action::ActionType::FIXUP;
        } else if (type_str == "reword") {
            type = core::action::ActionType::REWORD;
        } else if (type_str == "squash") {
            type = core::action::ActionType::SQUASH;
        } else {
            return false;
        }

        auto hash = el.attribute("hash").toStdString();
        if (hash.empty()) {
            return false;
        }

        core::git::commit_t action_commit;
        if (!core::git::get_commit_from_hash(action_commit, hash.c_str(), repo)) {
            return false;
        }

        auto msg = load_message(el);

        save_data.actions.emplace_back(core::action::Action(type, std::move(action_commit)), msg);
    }

    return true;
}

void save_conflicts(const core::conflict::ConflictManager& manager, QDomElement& root, QDomDocument& doc) {

    QDomElement conflicts = doc.createElement(CONFLICTS_NODE);

    for (auto&& [entry, blob] : manager.get_conflicts()) {
        QDomElement conflict = doc.createElement(CONFLICT_NODE);

        conflict.setAttribute("their", QString::fromStdString(entry.their_id));
        conflict.setAttribute("our", QString::fromStdString(entry.our_id));
        conflict.setAttribute("ancestor", QString::fromStdString(entry.ancestor_id));
        conflict.setAttribute("blob", QString::fromStdString(blob));

        conflicts.appendChild(conflict);
    }

    QDomElement conflict_commits = doc.createElement(CONFLICT_COMMITS_NODE);

    for (auto&& [conflict, tree] : manager.get_tree_conflicts()) {
        QDomElement commits = doc.createElement(CONFLICT_COMMIT_NODE);

        const auto* tree_id = git_tree_id(tree.get());

        commits.setAttribute("parent_tree", QString::fromStdString(conflict.parent_tree_id));
        commits.setAttribute("commit", QString::fromStdString(conflict.commit_id));
        commits.setAttribute(
            "tree", QString::fromStdString(core::git::format_oid_to_str<core::git::OID_SIZE>(tree_id))
        );

        conflict_commits.appendChild(commits);
    }

    root.appendChild(conflicts);
    root.appendChild(conflict_commits);
}

void load_conflicts(QDomElement& root, SaveData& save_data, git_repository* repo) {
    QDomElement conflicts = root.firstChildElement(CONFLICTS_NODE);
    if (conflicts.isNull()) {
        return;
    }

    for (QDomNode node = conflicts.firstChildElement(CONFLICT_NODE); !node.isNull();
         node          = node.nextSiblingElement(CONFLICT_NODE)) {

        QDomElement conflict = node.toElement();

        core::conflict::ConflictEntry entry;
        entry.their_id    = conflict.attribute("their").toStdString();
        entry.our_id      = conflict.attribute("our").toStdString();
        entry.ancestor_id = conflict.attribute("ancestor").toStdString();

        std::string blob = conflict.attribute("blob").toStdString();

        save_data.conflicts.emplace_back(entry, blob);
    }

    QDomElement conflict_commits = root.firstChildElement(CONFLICT_COMMITS_NODE);
    if (conflict_commits.isNull()) {
        return;
    }

    for (QDomNode node = conflicts.firstChildElement(CONFLICT_COMMIT_NODE); !node.isNull();
         node          = node.nextSiblingElement(CONFLICT_COMMIT_NODE)) {

        QDomElement commits = node.toElement();

        core::conflict::ConflictTrees entry;
        entry.parent_tree_id = commits.attribute("parent_tree").toStdString();
        entry.commit_id      = commits.attribute("commit").toStdString();

        auto tree_id = commits.attribute("tree").toStdString();

        git_oid oid;
        if (git_oid_fromstr(&oid, tree_id.c_str()) != 0) {
            continue;
        }

        core::git::tree_t tree;
        if (git_tree_lookup(&tree, repo, &oid) != 0) {
            continue;
        }

        save_data.conflict_trees.emplace_back(entry, std::move(tree));
    }
}

bool State::save(
    const std::filesystem::path& path,
    const AppState& state,
    const core::action::ActionsManager& actions,
    const core::conflict::ConflictManager& conflicts
) {

    QDomDocument doc;

    QDomProcessingInstruction header = doc.createProcessingInstruction("xml", R"(version="1.0" encoding="UTF-8")");
    doc.appendChild(header);

    QDomElement root = doc.createElement(ROOT_NODE);
    root.setAttribute("repo", QString::fromStdString(state.repo_path()));
    root.setAttribute("head", QString::fromStdString(state.rebase_head()));
    root.setAttribute("onto", QString::fromStdString(state.rebase_onto()));

    doc.appendChild(root);

    save_actions(actions, root, doc);

    save_conflicts(conflicts, root, doc);

    QFile file(QString::fromStdU32String(path.u32string()));
    if (!file.open(QFile::WriteOnly)) {
        return false;
    }

    QTextStream stream(&file);
    stream << doc.toString(2);

    return true;
}

std::optional<SaveData> State::load(const std::filesystem::path& path, git_repository** repo) {
    QFile file(QString::fromStdU32String(path.u32string()));
    if (!file.open(QFile::OpenModeFlag::ReadOnly)) {
        return std::nullopt;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        return std::nullopt;
    }

    SaveData save_data;

    // -- Repo ----------------------------------------------------------------
    QDomElement root = doc.documentElement();
    if (root.tagName() != ROOT_NODE || !root.hasAttribute("repo") || root.attribute("repo").isEmpty()) {
        return std::nullopt;
    }

    if (!root.hasAttribute("head") || !root.hasAttribute("onto")) {
        return std::nullopt;
    }

    save_data.repo_path = root.attribute("repo").toStdString();
    save_data.head      = root.attribute("head").toStdString();
    save_data.onto      = root.attribute("onto").toStdString();

    if (git_repository_open(repo, save_data.repo_path.c_str()) != 0) {
        return std::nullopt;
    }

    if (!load_actions(root, save_data, *repo)) {
        return std::nullopt;
    }

    load_conflicts(root, save_data, *repo);

    return save_data;
}
}
