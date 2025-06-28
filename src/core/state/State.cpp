#include "core/state/State.h"
#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/git/types.h"
#include "logging/Log.h"

#include <filesystem>
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

constexpr const char* ROOT_NODE    = "save_data";
constexpr const char* ROOT_COMMIT  = "root";
constexpr const char* ACTIONS_NODE = "actions";
constexpr const char* ACTION_NODE  = "action";

std::string load_message(const QDomElement& action) {
    QDomNodeList nodes = action.elementsByTagName("line");

    QStringList lines;
    for (auto&& line : nodes) {
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

    auto& manager = action::ActionsManager::get();

    auto* cmt = manager.get_root_commit();

    QDomElement root_commit = doc.createElement(ROOT_COMMIT);
    root_commit.setAttribute("hash", QString::fromStdString(git::format_oid_to_str<40>(git_commit_id(cmt))));
    root.appendChild(root_commit);

    QDomElement actions = doc.createElement(ACTIONS_NODE);

    for (auto&& act : manager) {
        QDomElement action = doc.createElement(ACTION_NODE);

        const git_oid& id = act.get_oid();
        action.setAttribute("hash", QString::fromStdString(git::format_oid_to_str<40>(&id)));

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

    // -- Root commit ---------------------------------------------------------
    QDomElement root_commit = root.firstChildElement(ROOT_COMMIT);
    if (root_commit.isNull() || !root_commit.hasAttribute("hash") || root_commit.attribute("hash").isEmpty()) {
        return std::nullopt;
    }

    auto root_commit_hash = root_commit.attribute("hash").toStdString();

    if (!git::get_commit_from_hash(save_data.root, root_commit_hash.c_str(), *repo)) {
        return std::nullopt;
    }

    // -- Actions -------------------------------------------------------------
    QDomElement actions = root.firstChildElement(ACTIONS_NODE);
    if (actions.isNull()) {
        return std::nullopt;
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
            return std::nullopt;
        }

        auto hash = el.attribute("hash").toStdString();
        if (hash.empty()) {
            return std::nullopt;
        }

        git::commit_t action_commit;
        if (!git::get_commit_from_hash(action_commit, hash.c_str(), *repo)) {
            return std::nullopt;
        }

        auto msg = load_message(el);

        save_data.actions.emplace_back(action::Action(type, std::move(action_commit)), msg);
    }

    return save_data;
}
}
