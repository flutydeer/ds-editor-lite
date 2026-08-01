#include "UI/Views/Common/EditorShortcutUtils.h"
#include "UI/Views/Common/EditorMenuPreviewGuard.h"

#include <QApplication>
#include <QEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

    int failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

} // namespace

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);

    QWidget owner;
    auto *layout = new QVBoxLayout(&owner);
    auto *canvas = new QWidget(&owner);
    auto *lineEdit = new QLineEdit(&owner);
    layout->addWidget(canvas);
    layout->addWidget(lineEdit);
    owner.show();

    int activationCount = 0;
    auto *shortcut = EditorShortcutUtils::add(&owner, QKeySequence::Delete, &owner,
                                              [&activationCount] { ++activationCount; });

    canvas->setFocus();
    application.processEvents();
    expect(shortcut->isEnabled(), "editor shortcut must be enabled for canvas focus");

    lineEdit->setFocus();
    application.processEvents();
    expect(!shortcut->isEnabled(), "editor shortcut must not intercept QLineEdit input");
    expect(EditorShortcutUtils::isTextInput(new QTextEdit(&owner)),
           "QTextEdit must be recognized as text input");
    expect(EditorShortcutUtils::isTextInput(new QPlainTextEdit(&owner)),
           "QPlainTextEdit must be recognized as text input");
    expect(EditorShortcutUtils::isTextInput(new QSpinBox(&owner)),
           "QAbstractSpinBox subclasses must be recognized as text input");
    expect(activationCount == 0, "focus changes must not activate editor commands");

    QMenu menu;
    auto *pasteAction = menu.addAction(QStringLiteral("Paste"));
    menu.addAction(QStringLiteral("Other"));
    int previewClearCount = 0;
    new EditorMenuPreviewGuard(&menu, pasteAction, [&previewClearCount] { ++previewClearCount; });
    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(&menu, &leaveEvent);
    expect(previewClearCount == 1, "leaving a menu must clear its paste preview");

    return failures == 0 ? 0 : 1;
}
