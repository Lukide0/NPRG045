#pragma once

#include <QPlainTextEdit>
#include <QWidget>

namespace gui::widget {

class ConflictEditor : public QPlainTextEdit {
public:
    using QPlainTextEdit::QPlainTextEdit;
};

}
