#include "tst_gui_components.h"

#include <lite/GUI/Controls/FileSelector.h>
#include <lite/GUI/Controls/PathEditor.h>
#include <lite/GUI/Controls/PathListWidget.h>

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QMimeData>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

namespace {
    void dropPaths(QWidget *target, const QList<QUrl> &urls, bool accepted = true) {
        QMimeData mime;
        mime.setUrls(urls);
        const QPoint point(10, 10);
        QDragEnterEvent enter(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(target, &enter);
        QCOMPARE(enter.isAccepted(), accepted);
        if (!accepted)
            return;
        QDragMoveEvent move(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        // Qt carries the accepted enter action into subsequent native drag moves.
        move.setDropAction(enter.dropAction());
        move.accept();
        QApplication::sendEvent(target, &move);
        QVERIFY(move.isAccepted());
        QDropEvent drop(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(target, &drop);
        QVERIFY(drop.isAccepted());
    }
}

void GuiComponentTests::pathEditorMovesAndDeletesTheSelectedDirectories() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QStringList paths;
    QList<QUrl> urls;
    for (const auto &name : {QStringLiteral("First"), QStringLiteral("Second"),
                             QStringLiteral("Third"), QStringLiteral("Fourth")}) {
        QVERIFY(QDir(root.path()).mkdir(name));
        const auto path = root.filePath(name);
        paths.append(QDir::toNativeSeparators(path));
        urls.append(QUrl::fromLocalFile(path));
    }
    PathEditor editor;
    editor.resize(640, 320);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow());
    auto *list = editor.listWidget();
    QVERIFY(list);
    QPushButton *up = nullptr;
    QPushButton *down = nullptr;
    QPushButton *remove = nullptr;
    for (auto *button : editor.findChildren<QPushButton *>()) {
        if (button->text() == PathEditor::tr("Move &Up"))
            up = button;
        else if (button->text() == PathEditor::tr("Move D&own"))
            down = button;
        else if (button->text() == PathEditor::tr("&Delete"))
            remove = button;
    }
    QVERIFY(up && down && remove);
    QSignalSpy changed(&editor, &PathEditor::pathsChanged);
    dropPaths(list->viewport(), urls);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(editor.paths(), paths);
    QCOMPARE(changed.count(), 1);
    const auto clickRow = [&](int row, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QTest::mouseClick(list->viewport(), Qt::LeftButton, modifiers,
                          list->visualItemRect(list->item(row)).center());
    };
    clickRow(1);
    clickRow(2, Qt::ShiftModifier);
    QCOMPARE(list->selectedItems().size(), 2);
    QTest::mouseClick(up, Qt::LeftButton);
    QCOMPARE(editor.paths(), (QStringList{paths[1], paths[2], paths[0], paths[3]}));
    QCOMPARE(list->selectedItems().size(), 2);
    const auto atTop = changed.count();
    QTest::mouseClick(up, Qt::LeftButton);
    QCOMPARE(changed.count(), atTop);
    QTest::mouseClick(down, Qt::LeftButton);
    QCOMPARE(editor.paths(), paths);
    QCOMPARE(list->selectedItems().size(), 2);
    QTest::mouseClick(remove, Qt::LeftButton);
    QCOMPARE(editor.paths(), (QStringList{paths[0], paths[3]}));
    QVERIFY(list->selectedItems().isEmpty());
    const auto afterDelete = changed.count();
    QTest::mouseClick(remove, Qt::LeftButton);
    QCOMPARE(changed.count(), afterDelete);
}

void GuiComponentTests::fileSelectorAcceptsTheFirstSuitableLocalDrop_data() {
    QTest::addColumn<bool>("directory");
    QTest::addColumn<bool>("restrictExtension");
    QTest::newRow("directory") << true << false;
    QTest::newRow("model-file") << false << true;
    QTest::newRow("executable-file") << false << false;
}

void GuiComponentTests::fileSelectorAcceptsTheFirstSuitableLocalDrop() {
    QFETCH(bool, directory);
    QFETCH(bool, restrictExtension);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto filePath = root.filePath(QStringLiteral("engine.ONNX"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("fixture"), 7);
    file.close();
    FileSelector selector;
    selector.setDirMode(directory);
    if (restrictExtension)
        selector.setFileDropExtensions({QStringLiteral("onnx")});
    selector.setPath(QStringLiteral("original"));
    selector.resize(600, 50);
    selector.show();
    QSignalSpy changed(&selector, &FileSelector::pathChanged);
    const QUrl remote(QStringLiteral("https://example.invalid/engine.onnx"));
    dropPaths(&selector, {remote}, false);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(selector.path(), QStringLiteral("original"));
    QVERIFY(changed.isEmpty());
    dropPaths(&selector, {remote, QUrl::fromLocalFile(filePath), QUrl::fromLocalFile(root.path())});
    if (QTest::currentTestFailed())
        return;
    const auto expected = directory ? root.path() : filePath;
    QCOMPARE(selector.path(), expected);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().first().toString(), expected);
}
