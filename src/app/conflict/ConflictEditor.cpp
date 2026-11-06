#include "app/conflict/ConflictEditor.h"
#include "core/Result.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace app::conflict {

void ConflictEditor::setEditor(QString editor) { m_template = editor.trimmed(); }

void ConflictEditor::setStyle(ConflictStyle style) { m_style = style; }

void ConflictEditor::load(QSettings& settings) {
    settings.beginGroup("ConflictEditor");
    {
        // templated command
        m_template = settings.value("template", QString { }).value<QString>();

        // result file style
        m_style = static_cast<ConflictStyle>(settings.value("style", static_cast<int>(ConflictStyle::ZDIFF3)).toInt());
    }
    settings.endGroup();
}

void ConflictEditor::save(QSettings& settings) {

    settings.beginGroup("ConflictEditor");
    settings.setValue("template", m_template);
    settings.setValue("style", static_cast<int>(m_style));
    settings.endGroup();
}

[[nodiscard]] QString ConflictEditor::commandTemplate() const { return m_template; }

core::Result<ConflictEditor::Editor> ConflictEditor::validate(QString editor) {

    editor = editor.trimmed();
    if (editor.isEmpty()) {

        return std::unexpected<core::Error>({
            .title   = "Conflict editor not configured",
            .message = "Enter a command for your preferred conflict editor",
        });
    }

    const QStringList arguments = QProcess::splitCommand(editor);

    if (arguments.isEmpty() || arguments.first().isEmpty()) {
        return std::unexpected<core::Error>({
            .title   = "Invalid conflict editor",
            .message = "The editor command appears to be incomplete",
        });
    }

    const QString program = QStandardPaths::findExecutable(arguments.first());

    if (program.isEmpty()) {
        return std::unexpected<core::Error>({
            .title   = "Invalid conflict editor",
            .message = std::format(
                "We couldn't find \"{}\". Check that the editor is installed "
                "and that its command or path is correct",
                arguments.first().toStdString()
            ),

        });
    }

    const QStringList all {
        "<BASE>",
        "<OUR>",
        "<THEIR>",
        "<RESULT>",
    };

    if (!editor.contains("<RESULT>")) {
        return std::unexpected<core::Error>({
            .title   = "Invalid editor command",
            .message = "Add the <RESULT> placeholder to the editor command.",
        });
    }

    static const QRegularExpression placeholder_regex(R"(<[^<>]+>)");

    Editor parsed {
        .program   = arguments.first(),
        .arguments = arguments.mid(1),
    };

    auto matches = placeholder_regex.globalMatch(editor);
    while (matches.hasNext()) {
        const QString placeholder = matches.next().captured();

        if (!all.contains(placeholder)) {
            return std::unexpected<core::Error>({
                .title   = "Unknown conflict editor placeholder",
                .message = std::format("Unsupported placeholder: {}", placeholder.toStdString()),

            });
        }
    }

    return parsed;
}

core::Result<ConflictEditor::Editor> ConflictEditor::createEditor(const EditorValues& files) const {
    auto parsed = validate(m_template);
    if (!parsed.has_value()) {
        return std::unexpected<core::Error>(parsed.error());
    }

    auto editor = std::move(parsed).value();

    for (auto& argument : editor.arguments) {
        argument.replace("<BASE>", files.base);
        argument.replace("<OUR>", files.our);
        argument.replace("<THEIR>", files.their);
        argument.replace("<RESULT>", files.result);
    }

    return editor;
}
}
