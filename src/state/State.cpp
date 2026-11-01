#include "state/State.h"

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"

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

std::optional<std::string> load_message(const QDomElement& action) {
    QDomNodeList nodes = action.elementsByTagName("line");
    if (nodes.isEmpty()) {
        return std::nullopt;
    }

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

void save_actions(const core::rebase::RebasePlan& plan, QDomElement& root, QDomDocument& doc) {

    auto* cmt = plan.root_commit();

    QDomElement root_commit = doc.createElement(ROOT_COMMIT);
    root_commit.setAttribute(
        "hash", QString::fromStdString(core::git::format_oid_to_str<OID_SIZE>(git_commit_id(cmt)))
    );
    root.appendChild(root_commit);

    QDomElement actions = doc.createElement(ACTIONS_NODE);

    for (auto&& act : plan) {
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

core::Result<void> load_actions(QDomElement& root, SaveData& save_data, git_repository* repo) {
    core::Error invalid_save {
        .title   = "Unable to load save",
        .message = "The save file is invalid or corrupted.",
    };

    // -- Root commit ---------------------------------------------------------
    QDomElement root_commit = root.firstChildElement(ROOT_COMMIT);
    if (root_commit.isNull() || !root_commit.hasAttribute("hash") || root_commit.attribute("hash").isEmpty()) {
        return std::unexpected<core::Error>(invalid_save);
    }

    git_commit* commit = save_data.graph.first_node().commit;

    save_data.actions.set_root_commit(commit);

    auto root_commit_hash = root_commit.attribute("hash").toStdString();
    if (root_commit_hash != core::git::format_oid_to_str<core::git::OID_SIZE>(git_commit_id(commit))) {
        return std::unexpected<core::Error>({
            .title   = "Unable to load save",
            .message = "The root commit does not match",
        });
    }

    // -- Actions -------------------------------------------------------------
    QDomElement actions = root.firstChildElement(ACTIONS_NODE);
    if (actions.isNull()) {
        return std::unexpected<core::Error>(invalid_save);
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
            return std::unexpected<core::Error>(invalid_save);
        }

        auto hash = el.attribute("hash").toStdString();
        if (hash.empty()) {
            return std::unexpected<core::Error>(invalid_save);
        }

        core::git::commit_t action_commit;
        if (!core::git::get_commit_from_hash(action_commit, hash.c_str(), repo)) {
            return std::unexpected<core::Error>({
                .title   = "Unable to load save",
                .message = "The action commit does not exists",
            });
        }

        auto msg = load_message(el);
        save_data.actions.append(core::action::Action(type, std::move(action_commit), msg));
    }

    return { };
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

core::Result<void> load_conflicts(QDomElement& root, SaveData& save_data, git_repository* repo) {
    QDomElement conflicts = root.firstChildElement(CONFLICTS_NODE);
    if (!conflicts.isNull()) {
        for (QDomNode node = conflicts.firstChildElement(CONFLICT_NODE); !node.isNull();
             node          = node.nextSiblingElement(CONFLICT_NODE)) {

            QDomElement conflict = node.toElement();

            core::conflict::ConflictEntry entry;
            entry.their_id    = conflict.attribute("their").toStdString();
            entry.our_id      = conflict.attribute("our").toStdString();
            entry.ancestor_id = conflict.attribute("ancestor").toStdString();

            std::string blob = conflict.attribute("blob").toStdString();

            save_data.conflicts.add_resolution(entry, blob);
        }
    }

    QDomElement conflict_commits = root.firstChildElement(CONFLICT_COMMITS_NODE);
    if (!conflict_commits.isNull()) {
        for (QDomNode node = conflict_commits.firstChildElement(CONFLICT_COMMIT_NODE); !node.isNull();
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

            save_data.conflicts.add_trees_resolution(entry, std::move(tree));
        }
    }

    return { };
}

bool State::save(const std::filesystem::path& path, const core::rebase::RebaseSession& rebase) {
    QDomDocument doc;

    const auto& repo = rebase.repo();
    const auto& info = rebase.plan().info();

    QDomProcessingInstruction header = doc.createProcessingInstruction("xml", R"(version="1.0" encoding="UTF-8")");
    doc.appendChild(header);

    QDomElement root = doc.createElement(ROOT_NODE);
    root.setAttribute("repo", QString::fromStdString(repo.path()));
    root.setAttribute("head", QString::fromStdString(info.head));
    root.setAttribute("onto", QString::fromStdString(info.onto));

    doc.appendChild(root);

    save_actions(rebase.plan(), root, doc);

    save_conflicts(rebase.conflicts(), root, doc);

    QFile file(QString::fromStdU32String(path.u32string()));
    if (!file.open(QFile::WriteOnly)) {
        return false;
    }

    QTextStream stream(&file);
    stream << doc.toString(2);

    return true;
}

core::Result<SaveData> State::load(const std::filesystem::path& path) {
    QFile file(QString::fromStdU32String(path.u32string()));
    if (!file.open(QFile::OpenModeFlag::ReadOnly)) {
        return std::unexpected<core::Error>({
            .title   = "Unable to load save",
            .message = "Could not open save file",
        });
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        return std::unexpected<core::Error>({
            .title   = "Unable to load save",
            .message = "Could not read save file",
        });
    }

    core::Error invalid_save {
        .title   = "Unable to load save",
        .message = "The save file is invalid or corrupted.",
    };
    // -- Repo ----------------------------------------------------------------
    QDomElement root = doc.documentElement();
    if (root.tagName() != ROOT_NODE || !root.hasAttribute("repo") || root.attribute("repo").isEmpty()) {
        return std::unexpected<core::Error>(invalid_save);
    }

    if (!root.hasAttribute("head") || !root.hasAttribute("onto")) {
        return std::unexpected<core::Error>(invalid_save);
    }

    auto repo = core::git::RepoSession::open(root.attribute("repo").toStdString());
    if (!repo.has_value()) {
        return std::unexpected<core::Error>({
            .title   = "Unable to load save",
            .message = repo.error().message,
        });
    }

    SaveData save_data;
    save_data.info.head = root.attribute("head").toStdString();
    save_data.info.onto = root.attribute("onto").toStdString();
    save_data.repo      = std::move(*repo);

    auto graph = core::git::GitGraph<void>::create(
        save_data.info.head.c_str(), save_data.info.onto.c_str(), save_data.repo.repo()
    );
    if (!graph.has_value()) {
        return std::unexpected<core::Error>({
            .title   = "Unable to load save",
            .message = "Could not find commits",
        });
    }

    save_data.graph = std::move(*graph);

    auto res = load_actions(root, save_data, save_data.repo.repo());
    if (!res.has_value()) {
        return std::unexpected<core::Error>(res.error());
    }

    res = load_conflicts(root, save_data, save_data.repo.repo());
    if (!res.has_value()) {
        return std::unexpected<core::Error>(res.error());
    }

    return save_data;
}
}
