#pragma once

#include "app/conflict/ConflictEditor.h"
#include "core/conflict/ConflictManager.h"

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <string>

namespace app::conflict {

struct ConflictFileId {
    std::string path;
    core::conflict::ConflictEntry entry;

    bool operator==(const ConflictFileId&) const = default;
};

inline std::size_t qHash(const ConflictFileId& id, std::size_t seed = 0) noexcept {
    return qHashMulti(seed, id.path, id.entry.ancestor_id, id.entry.our_id, id.entry.their_id);
}

enum class ConflictResolutionType {
    Deleted,
    Modified,
};

class ConflictEditorSession : public QObject {
    Q_OBJECT

public:
    ConflictEditorSession(
        ConflictFileId id, ConflictEditor::Editor editor, ConflictEditor::EditorValues files, QObject* parent = nullptr
    );

    void start();
    void discard();

    [[nodiscard]] const ConflictFileId& conflictId() const { return m_id; }

    [[nodiscard]] bool isDiscarded() const { return m_discarded; }

    [[nodiscard]] bool isFinished() const { return m_finished; }

signals:
    void closedEditor();
    void failed(QString message);
    void stopped();

private:
    ConflictFileId m_id;
    ConflictEditor::EditorValues m_files;
    ConflictEditor::Editor m_editor;
    QProcess m_process;

    bool m_finished  = false;
    bool m_discarded = false;

    void onFinished(int exit_code, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);

    void finishWithError(const QString& message);
    void finish();
};

}
