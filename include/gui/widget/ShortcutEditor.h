#pragma once

#include "App.h"
#include <QAction>
#include <QDialog>
#include <QKeySequenceEdit>
#include <QMap>
#include <QPushButton>
#include <QTableWidget>

namespace gui::widget {

class ShortcutEditor : public QWidget {
    Q_OBJECT

public:
    explicit ShortcutEditor(QWidget* parent = nullptr);

private slots:
    void onEditShortcut(int row, int column);
    void onResetToDefaults();

private:
    QMap<QString, App::ShortcutAction>& m_actions;
    QTableWidget* m_table;
    QPushButton* m_btn_reset;

    void setup();
    void populateTable();
};

class KeySequenceEditFilter : public QObject {
    Q_OBJECT
public:
    explicit KeySequenceEditFilter(QObject* parent = nullptr)
        : QObject(parent) { }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() != QEvent::KeyPress) {
            return QObject::eventFilter(obj, event);
        }

        auto* key_event = static_cast<QKeyEvent*>(event);
        if (key_event->key() != Qt::Key_Escape) {
            return QObject::eventFilter(obj, event);
        }

        auto* editor = qobject_cast<QKeySequenceEdit*>(obj);
        if (editor != nullptr) {
            editor->clear();
        }
        return true;
    }
};

}
