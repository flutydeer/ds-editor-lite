#include "tst_application_gui.h"

#include "UI/Dialogs/Base/MessageDialog.h"

#include <QCheckBox>
#include <QLabel>
#include <QLayout>
#include <QScreen>
#include <QSignalSpy>
#include <QtTest/QTest>

void ApplicationGuiTests::dialogMessagesWrap_data() {
    QTest::addColumn<QString>("message");
    QTest::addColumn<int>("dialogWidth");
    const auto cacheMessage =
        QStringLiteral("This will delete 123 cache file(s) not used by the current project. "
                       "Files used by undo history and current playback will be kept.");
    QTest::newRow("short") << QStringLiteral("Continue?") << 0;
    QTest::newRow("cache-confirmation") << cacheMessage << 0;
    QTest::newRow("narrow-cache-confirmation") << cacheMessage << 320;
    QTest::newRow("multiline") << QStringLiteral("First line.\n\n") + cacheMessage << 360;
    QTest::newRow("translated")
        << QString::fromUtf8(
               "这将删除当前工程未使用的 123 个缓存文件。撤销历史和当前播放使用的文件将会保留。")
        << 320;
}

void ApplicationGuiTests::dialogMessagesWrap() {
    QFETCH(QString, message);
    QFETCH(int, dialogWidth);
    MessageDialog dialog(QStringLiteral("Clear cache"), message);
    dialog.addButton(QStringLiteral("Cancel"), 0);
    dialog.addAccentButton(QStringLiteral("OK"), 1);
    auto *checkBox = dialog.addCheckBox(QStringLiteral("Remember my choice"));
    QVERIFY(checkBox);
    if (dialogWidth)
        dialog.setFixedWidth(dialogWidth);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *label = dialog.findChild<QLabel *>(QStringLiteral("DialogMessageLabel"));
    QVERIFY(label);
    QVERIFY(label->wordWrap());
    QCOMPARE(label->text(), message);
    QTRY_VERIFY(label->height() >= label->heightForWidth(label->width()));
    QVERIFY(dialog.rect().contains(label->mapTo(&dialog, label->rect().bottomRight())));
    QVERIFY(label->mapTo(&dialog, label->rect().bottomLeft()).y() <
            checkBox->mapTo(&dialog, QPoint()).y());
    QVERIFY(dialog.width() <= dialog.screen()->availableGeometry().width());
    QVERIFY(dialog.height() <= dialog.screen()->availableGeometry().height());
    checkBox->setFocus();
    QTest::keyClick(checkBox, Qt::Key_Space);
    QVERIFY(dialog.isCheckBoxChecked());
    QSignalSpy rejected(&dialog, &QDialog::rejected);
    QTest::keyClick(&dialog, Qt::Key_Escape);
    QCOMPARE(rejected.count(), 1);

    dialog.setTitle(message);
    dialog.setMessage(message + QStringLiteral("\nPlease confirm your choice."));
    dialog.adjustSize();
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *title = dialog.findChild<QLabel *>(QStringLiteral("DialogTitleLabel"));
    QVERIFY(title);
    QVERIFY(title->wordWrap());
    QTRY_VERIFY(title->height() >= title->heightForWidth(title->width()));
    QTRY_VERIFY(label->height() >= label->heightForWidth(label->width()));
    QSignalSpy accepted(&dialog, &QDialog::accepted);
    for (auto *button : dialog.findChildren<Button *>()) {
        if (button->text() == QStringLiteral("OK"))
            QTest::mouseClick(button, Qt::LeftButton);
    }
    QCOMPARE(accepted.count(), 1);
}
