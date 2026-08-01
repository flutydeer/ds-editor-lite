#ifndef EDITORSHORTCUTUTILS_H
#define EDITORSHORTCUTUTILS_H

#include <QAbstractSpinBox>
#include <QApplication>
#include <QKeySequence>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QTextEdit>
#include <QWidget>

#include <utility>

namespace EditorShortcutUtils {

    inline bool isTextInput(const QWidget *widget) {
        return qobject_cast<const QLineEdit *>(widget) || qobject_cast<const QTextEdit *>(widget) ||
               qobject_cast<const QPlainTextEdit *>(widget) ||
               qobject_cast<const QAbstractSpinBox *>(widget);
    }

    template <typename Receiver, typename Slot>
    QShortcut *add(QWidget *owner, const QKeySequence &key, Receiver *receiver, Slot &&slot) {
        auto *shortcut = new QShortcut(key, owner);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        shortcut->setEnabled(!isTextInput(QApplication::focusWidget()));
        QObject::connect(qApp, &QApplication::focusChanged, shortcut,
                         [shortcut](QWidget *, QWidget *current) {
                             shortcut->setEnabled(!isTextInput(current));
                         });
        QObject::connect(shortcut, &QShortcut::activated, receiver, std::forward<Slot>(slot));
        return shortcut;
    }

} // namespace EditorShortcutUtils

#endif // EDITORSHORTCUTUTILS_H
