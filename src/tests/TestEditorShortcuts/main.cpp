#include "UI/Views/Common/EditorShortcutUtils.h"
#include "UI/Views/Common/EditorMenuPreviewGuard.h"

#include <QtTest/QTest>
#include <QDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
    class SpaceOverrideButton final : public QPushButton {
    protected:
        bool event(QEvent *event) override {
            if (event->type() == QEvent::ShortcutOverride &&
                static_cast<QKeyEvent *>(event)->key() == Qt::Key_Space) {
                event->accept();
                return true;
            }
            return QPushButton::event(event);
        }
    };

    class SpaceKeyWidget final : public QWidget {
    public:
        int presses = 0;

    protected:
        void keyPressEvent(QKeyEvent *event) override {
            if (event->key() == Qt::Key_Space)
                ++presses;
            QWidget::keyPressEvent(event);
        }
    };
}

class EditorShortcutsTests final : public QObject {
    Q_OBJECT

private slots:

    void editingFocusProtectsTextInput() {
        QWidget owner;
        auto *layout = new QVBoxLayout(&owner);
        auto *canvas = new QWidget;
        canvas->setFocusPolicy(Qt::StrongFocus);
        auto *lineEdit = new QLineEdit;
        layout->addWidget(canvas);
        layout->addWidget(lineEdit);
        int activations = 0;
        auto *shortcut =
            EditorShortcutUtils::add(&owner, QKeySequence::Delete, &owner, [&] { ++activations; });
        owner.show();
        owner.activateWindow();
        canvas->setFocus();
        QTRY_VERIFY(shortcut->isEnabled());
        QTest::keyClick(canvas, Qt::Key_Delete);
        QTRY_COMPARE(activations, 1);
        lineEdit->setText(QStringLiteral("ab"));
        lineEdit->setCursorPosition(0);
        lineEdit->setFocus();
        QTRY_VERIFY(!shortcut->isEnabled());
        QTest::keyClick(lineEdit, Qt::Key_Delete);
        QCOMPARE(lineEdit->text(), QStringLiteral("b"));
        QCOMPARE(activations, 1);
        QTextEdit richText;
        QPlainTextEdit plainText;
        QSpinBox number;
        QVERIFY(EditorShortcutUtils::isTextInput(&richText));
        QVERIFY(EditorShortcutUtils::isTextInput(&plainText));
        QVERIFY(EditorShortcutUtils::isTextInput(&number));
    }

    void applicationShortcutOverridesTools_data() {
        QTest::addColumn<bool>("detached");
        QTest::newRow("owner") << false;
        QTest::newRow("detached-panel") << true;
    }

    void applicationShortcutOverridesTools() {
        QFETCH(bool, detached);
        QWidget owner;
        QWidget panel;
        auto *window = detached ? &panel : &owner;
        auto *layout = new QVBoxLayout(window);
        auto *button = new SpaceOverrideButton;
        layout->addWidget(button);
        int activations = 0;
        int clicks = 0;
        EditorShortcutUtils::addApplication(
            &owner, QKeySequence(Qt::Key_Space),
            [&](const QWidget *candidate) { return candidate == &owner || candidate == &panel; },
            &owner, [&] { ++activations; });
        connect(button, &QPushButton::clicked, &owner, [&] { ++clicks; });
        window->show();
        window->activateWindow();
        button->setFocus();
        QTRY_VERIFY(button->hasFocus());
        QTest::keyClick(button, Qt::Key_Space);
        QTRY_COMPARE(activations, 1);
        QCOMPARE(clicks, 0);
    }

    void applicationShortcutPreservesTextInput() {
        QWidget owner;
        auto *layout = new QVBoxLayout(&owner);
        auto *edit = new QLineEdit;
        layout->addWidget(edit);
        int activations = 0;
        EditorShortcutUtils::addApplication(
            &owner, QKeySequence(Qt::Key_Space),
            [&](const QWidget *window) { return window == &owner; }, &owner,
            [&] { ++activations; });
        owner.show();
        owner.activateWindow();
        edit->setFocus();
        QTRY_VERIFY(edit->hasFocus());
        QTest::keyClick(edit, Qt::Key_Space);
        QCOMPARE(edit->text(), QStringLiteral(" "));
        QCOMPARE(activations, 0);
    }

    void applicationShortcutPreservesOtherWindows_data() {
        QTest::addColumn<int>("kind");
        QTest::newRow("unrelated") << 0;
        QTest::newRow("dialog") << 1;
        QTest::newRow("popup") << 2;
    }

    void applicationShortcutPreservesOtherWindows() {
        QFETCH(int, kind);
        QWidget owner;
        QWidget unrelated;
        QDialog dialog(&owner);
        QWidget popup(nullptr, Qt::Popup);
        QWidget *window = kind == 0 ? &unrelated : kind == 1 ? &dialog : &popup;
        auto *layout = new QVBoxLayout(window);
        auto *control = new SpaceKeyWidget;
        control->setFocusPolicy(Qt::StrongFocus);
        layout->addWidget(control);
        int activations = 0;
        EditorShortcutUtils::addApplication(
            &owner, QKeySequence(Qt::Key_Space),
            [&](const QWidget *candidate) { return candidate == &owner; }, &owner,
            [&] { ++activations; });
        owner.show();
        window->show();
        window->activateWindow();
        control->setFocus();
        QTRY_VERIFY(control->hasFocus());
        QTest::keyClick(control, Qt::Key_Space);
        QCOMPARE(activations, 0);
        QCOMPARE(control->presses, 1);
    }

    void disablingShortcutRestoresButtonInput() {
        QWidget owner;
        auto *layout = new QVBoxLayout(&owner);
        auto *button = new SpaceOverrideButton;
        layout->addWidget(button);
        int activations = 0;
        int clicks = 0;
        auto *shortcut = EditorShortcutUtils::addApplication(
            &owner, QKeySequence(Qt::Key_Space),
            [&](const QWidget *window) { return window == &owner; }, &owner,
            [&] { ++activations; });
        connect(button, &QPushButton::clicked, &owner, [&] { ++clicks; });
        shortcut->setEnabled(false);
        owner.show();
        owner.activateWindow();
        button->setFocus();
        QTRY_VERIFY(button->hasFocus());
        QTest::keyClick(button, Qt::Key_Space);
        QTRY_COMPARE(clicks, 1);
        QCOMPARE(activations, 0);
    }

    void leavingMenuClearsPastePreview() {
        QMenu menu;
        auto *paste = menu.addAction(QStringLiteral("Paste"));
        menu.addAction(QStringLiteral("Other"));
        int cleared = 0;
        new EditorMenuPreviewGuard(&menu, paste, [&] { ++cleared; });
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&menu, &leave);
        QCOMPARE(cleared, 1);
    }
};

QTEST_MAIN(EditorShortcutsTests)
#include "main.moc"
