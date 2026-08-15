#include "UI/Views/Common/EditorShortcutUtils.h"
#include "UI/Views/Common/EditorMenuPreviewGuard.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>
#include <QtTest/QTest>

namespace {

    int failures = 0;

    class SpaceOverrideButton final : public QPushButton {
    protected:
        bool event(QEvent *event) override {
            if (event->type() == QEvent::ShortcutOverride) {
                const auto *keyEvent = static_cast<QKeyEvent *>(event);
                if (keyEvent->key() == Qt::Key_Space) {
                    event->accept();
                    return true;
                }
            }
            return QPushButton::event(event);
        }
    };

    class SpaceKeyWidget final : public QWidget {
    public:
        int spacePressCount = 0;

    protected:
        void keyPressEvent(QKeyEvent *event) override {
            if (event->key() == Qt::Key_Space)
                ++spacePressCount;
            QWidget::keyPressEvent(event);
        }
    };

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
    auto *dockedToolButton = new SpaceOverrideButton;
    layout->addWidget(canvas);
    layout->addWidget(lineEdit);
    layout->addWidget(dockedToolButton);
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

    QWidget detachedWindow;
    auto *detachedLayout = new QVBoxLayout(&detachedWindow);
    auto *toolButton = new SpaceOverrideButton;
    auto *detachedLineEdit = new QLineEdit;
    detachedLayout->addWidget(toolButton);
    detachedLayout->addWidget(detachedLineEdit);
    detachedWindow.show();

    int applicationActivationCount = 0;
    int toolClickCount = 0;
    auto *applicationShortcut =
        EditorShortcutUtils::addApplication(&owner, QKeySequence(Qt::Key_Space), &owner,
                                            [&applicationActivationCount] {
                                                ++applicationActivationCount;
                                            });
    QObject::connect(dockedToolButton, &QPushButton::clicked, &owner,
                     [&toolClickCount] { ++toolClickCount; });
    QObject::connect(toolButton, &QPushButton::clicked, &owner,
                     [&toolClickCount] { ++toolClickCount; });

    owner.activateWindow();
    dockedToolButton->setFocus();
    application.processEvents();
    QTest::keyClick(dockedToolButton, Qt::Key_Space);
    application.processEvents();
    expect(applicationActivationCount == 1,
           "application shortcut must override focused tools in its owner window");
    expect(toolClickCount == 0, "space playback shortcut must not click a docked tool");

    detachedWindow.activateWindow();
    toolButton->setFocus();
    application.processEvents();
    QTest::keyClick(toolButton, Qt::Key_Space);
    application.processEvents();
    expect(applicationActivationCount == 2,
           "application shortcut must override focused tools in a detached window");
    expect(toolClickCount == 0, "space playback shortcut must not click a detached tool");

    detachedLineEdit->setFocus();
    application.processEvents();
    QTest::keyClick(detachedLineEdit, Qt::Key_Space);
    application.processEvents();
    expect(detachedLineEdit->text() == QStringLiteral(" "),
           "application shortcut must preserve space in text input");
    expect(applicationActivationCount == 2,
           "application shortcut must stay disabled while editing text");

    applicationShortcut->setEnabled(false);
    owner.activateWindow();
    dockedToolButton->setFocus();
    application.processEvents();
    QTest::keyClick(dockedToolButton, Qt::Key_Space);
    application.processEvents();
    expect(applicationActivationCount == 2,
           "disabled application shortcut must preserve embedded panel input");
    expect(toolClickCount == 1,
           "disabled application shortcut must allow embedded panel controls to handle space");
    applicationShortcut->setEnabled(true);

    QDialog dialog(&owner);
    auto *dialogLayout = new QVBoxLayout(&dialog);
    auto *dialogControl = new SpaceKeyWidget;
    dialogControl->setFocusPolicy(Qt::StrongFocus);
    dialogLayout->addWidget(dialogControl);
    dialog.show();
    dialog.activateWindow();
    dialogControl->setFocus();
    application.processEvents();
    QTest::keyClick(dialogControl, Qt::Key_Space);
    application.processEvents();
    expect(applicationActivationCount == 2,
           "application shortcut must preserve dialog key handling");
    expect(dialogControl->spacePressCount == 1,
           "dialog controls must receive their own space key press");
    dialog.close();
    application.processEvents();

    QWidget popup(nullptr, Qt::Popup);
    auto *popupLayout = new QVBoxLayout(&popup);
    auto *popupControl = new SpaceKeyWidget;
    popupControl->setFocusPolicy(Qt::StrongFocus);
    popupLayout->addWidget(popupControl);
    popup.show();
    popup.activateWindow();
    popupControl->setFocus();
    application.processEvents();
    QTest::keyClick(popupControl, Qt::Key_Space);
    application.processEvents();
    expect(applicationActivationCount == 2,
           "application shortcut must preserve popup key handling");
    expect(popupControl->spacePressCount == 1,
           "popup controls must receive their own space key press");
    popup.close();
    application.processEvents();

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
