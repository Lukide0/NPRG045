#include "app/conflict/ConflictEditorSession.h"

#include <utility>

namespace app::conflict {

ConflictEditorSession::ConflictEditorSession(
    ConflictFileId id, ConflictEditor::Editor editor, TemporaryConflictFiles files, QObject* parent
)
    : QObject(parent)
    , m_id(std::move(id))
    , m_files(std::move(files))
    , m_editor(std::move(editor)) {

    connect(&m_process, &QProcess::finished, this, &ConflictEditorSession::onFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &ConflictEditorSession::onErrorOccurred);
}

void ConflictEditorSession::start() {
    if (isFinished() || isDiscarded()) {
        return;
    }

    m_process.setProgram(m_editor.program);
    m_process.setArguments(m_editor.arguments);

    m_process.start();
}

void ConflictEditorSession::discard() {
    m_discarded = true;

    if (m_process.state() == QProcess::NotRunning) {
        finish();
    }
}

void ConflictEditorSession::onFinished(int exit_code, QProcess::ExitStatus status) {
    if (isFinished()) {
        return;
    }

    if (isDiscarded()) {
        finish();
        return;
    }

    if (status != QProcess::NormalExit || exit_code != 0) {
        finishWithError(QStringLiteral("The conflict editor exited with code %1").arg(exit_code));
        return;
    }

    if (!QFileInfo::exists(m_files.files.result)) {
        emit resolved({ }, ConflictResolutionType::Deleted);

        finish();
        return;
    }

    QFile file(m_files.files.result);
    if (!file.open(QIODevice::ReadOnly)) {
        finishWithError(QStringLiteral("Could not read the editor result: %1").arg(file.errorString()));
        return;
    }

    const QByteArray contents = file.readAll();

    if (file.error() != QFileDevice::NoError) {
        finishWithError(QStringLiteral("Could not read the complete editor result: %1").arg(file.errorString()));
        return;
    }

    emit resolved(contents, ConflictResolutionType::Modified);

    finish();
}

void ConflictEditorSession::onErrorOccurred(QProcess::ProcessError error) {

    if (isFinished() || error != QProcess::FailedToStart) {
        return;
    }

    finishWithError(m_process.errorString());
}

void ConflictEditorSession::finishWithError(const QString& message) {
    if (isFinished()) {
        return;
    }

    if (!isDiscarded()) {
        emit failed(message);
    }

    finish();
}

void ConflictEditorSession::finish() {
    if (isFinished()) {
        return;
    }

    m_finished = true;
    emit stopped();
}

}
