#include "core/state/State.h"
#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"
#include "logging/Log.h"

#include <filesystem>
#include <git2/oid.h>
#include <git2/tree.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <git2/commit.h>
#include <git2/repository.h>
#include <git2/types.h>

#include <QDomDocument>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>

namespace core::state {

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

    // TODO: replace invalid characters ('<', '>', ...)

    auto lines = msg.split(QRegularExpression("\n|\r\n"), Qt::KeepEmptyParts);
    for (auto&& line : lines) {
        auto line_el = doc.createElement("line");
        line_el.appendChild(doc.createTextNode(line));

        action.appendChild(line_el);
    }
}

void save_actions(QDomElement& root, QDomDocument& doc) {
    auto& manager = action::ActionsManager::get();

    auto* cmt = manager.get_root_commit();

    QDomElement root_commit = doc.createElement(ROOT_COMMIT);
    root_commit.setAttribute("hash", QString::fromStdString(git::format_oid_to_str<git::OID_SIZE>(git_commit_id(cmt))));
    root.appendChild(root_commit);

    QDomElement actions = doc.createElement(ACTIONS_NODE);

    for (auto&& act : manager) {
        QDomElement action = doc.createElement(ACTION_NODE);

        const git_oid& id = act.get_oid();
        action.setAttribute("hash", QString::fromStdString(git::format_oid_to_str<git::OID_SIZE>(&id)));

        switch (act.get_type()) {
        case action::ActionType::PICK:
            action.setAttribute("type", "pick");
            break;
        case action::ActionType::SQUASH:
            action.setAttribute("type", "squash");
            break;
        case action::ActionType::FIXUP:
            action.setAttribute("type", "fixup");
            break;
        case action::ActionType::REWORD:
            action.setAttribute("type", "reword");
            break;
        case action::ActionType::EDIT:
            action.setAttribute("type", "edit");
            break;
        case action::ActionType::DROP:
            action.setAttribute("type", "drop");
            break;
        }

        auto msg_id = act.get_msg_id();

        if (msg_id.is_value() && act.has_msg()) {
            const auto msg = QString::fromStdString(manager.get_msg(msg_id.value()));
            save_message(msg, doc, action);
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

    if (!git::get_commit_from_hash(save_data.root, root_commit_hash.c_str(), repo)) {
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
        action::ActionType type;

        if (type_str == "pick") {
            type = action::ActionType::PICK;
        } else if (type_str == "edit") {
            type = action::ActionType::EDIT;
        } else if (type_str == "drop") {
            type = action::ActionType::DROP;
        } else if (type_str == "fixup") {
            type = action::ActionType::FIXUP;
        } else if (type_str == "reword") {
            type = action::ActionType::REWORD;
        } else if (type_str == "squash") {
            type = action::ActionType::SQUASH;
        } else {
            return false;
        }

        auto hash = el.attribute("hash").toStdString();
        if (hash.empty()) {
            return false;
        }

        git::commit_t action_commit;
        if (!git::get_commit_from_hash(action_commit, hash.c_str(), repo)) {
            return false;
        }

        auto msg = load_message(el);

        save_data.actions.emplace_back(action::Action(type, std::move(action_commit)), msg);
    }

    return true;
}

void save_conflicts(QDomElement& root, QDomDocument& doc) {
    auto& manager = conflict::ConflictManager::get();

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

    for (auto&& [conflict, tree] : manager.get_commits_conflicts()) {
        QDomElement commits = doc.createElement(CONFLICT_COMMIT_NODE);

        const auto* tree_id = git_tree_id(tree.get());

        commits.setAttribute("parent", QString::fromStdString(conflict.parent_id));
        commits.setAttribute("child", QString::fromStdString(conflict.child_id));
        commits.setAttribute("tree", QString::fromStdString(git::format_oid_to_str<git::OID_SIZE>(tree_id)));

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

        conflict::ConflictEntry entry;
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

        conflict::ConflictCommits entry;
        entry.parent_id = commits.attribute("parent").toStdString();
        entry.child_id  = commits.attribute("child").toStdString();

        auto tree_id = commits.attribute("tree").toStdString();

        git_oid oid;
        if (git_oid_fromstr(&oid, tree_id.c_str()) != 0) {
            continue;
        }

        git::tree_t tree;
        if (git_tree_lookup(&tree, repo, &oid) != 0) {
            continue;
        }

        save_data.conflict_commits.emplace_back(entry, std::move(tree));
    }
}

bool State::save(
    const std::filesystem::path& path,
    const std::filesystem::path& repo,
    const std::string& head,
    const std::string& onto
) {

    QDomDocument doc;

    QDomProcessingInstruction header = doc.createProcessingInstruction("xml", R"(version="1.0" encoding="UTF-8")");
    doc.appendChild(header);

    QDomElement root = doc.createElement(ROOT_NODE);
    root.setAttribute("repo", QString::fromStdU32String(repo.u32string()));
    root.setAttribute("head", QString::fromStdString(head));
    root.setAttribute("onto", QString::fromStdString(onto));

    doc.appendChild(root);

    save_actions(root, doc);

    save_conflicts(root, doc);

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

    auto repo_path = root.attribute("repo").toStdString();

    save_data.head = root.attribute("head").toStdString();
    save_data.onto = root.attribute("onto").toStdString();

    if (git_repository_open(repo, repo_path.c_str()) != 0) {
        return std::nullopt;
    }

    if (!load_actions(root, save_data, *repo)) {
        return std::nullopt;
    }

    load_conflicts(root, save_data, *repo);

    return save_data;
}
}
