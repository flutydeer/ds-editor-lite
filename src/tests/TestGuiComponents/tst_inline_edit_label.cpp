#include "tst_gui_components.h"

#include <lite/GUI/Controls/InlineEditLabel.h>
#include <lite/GUI/Controls/InlineTextEditOverlay.h>
#include <lite/GUI/Controls/Menu.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

void GuiComponentTests::inlineLabelMigratesTheOverlayHost_data() {
    QTest::addColumn<bool>("popupFirst");
    QTest::addColumn<bool>("createBeforeMove");
    QTest::addColumn<bool>("fallbackHost");
    for (const bool popupFirst : {false, true}) {
        const auto direction =
            popupFirst ? QByteArray("popup-to-inline") : QByteArray("inline-to-popup");
        QTest::newRow((direction + "-first-edit").constData()) << popupFirst << false << false;
        QTest::newRow((direction + "-dormant-overlay").constData()) << popupFirst << true << false;
        QTest::newRow((direction + "-fallback-parent").constData()) << popupFirst << true << true;
    }
}

void GuiComponentTests::inlineLabelMigratesTheOverlayHost() {
    QFETCH(bool, popupFirst);
    QFETCH(bool, createBeforeMove);
    QFETCH(bool, fallbackHost);
    QWidget window;
    window.resize(500, 160);
    QWidget inlineHost(&window);
    inlineHost.setGeometry(0, 0, 450, 100);
    QWidget popupHost(&window, Qt::Popup);
    popupHost.resize(450, 100);
    QWidget *firstHost = popupFirst ? &popupHost : &inlineHost;
    QWidget *nextHost = popupFirst ? &inlineHost : &popupHost;
    InlineEditLabel label(firstHost);
    label.setGeometry(10, 10, 180, 28);
    label.setText(QStringLiteral("Original"));
    if (!fallbackHost)
        label.setOverlayParent(firstHost);
    window.show();
    firstHost->show();
    firstHost->window()->activateWindow();
    label.show();
    QSignalSpy commits(&label, &InlineEditLabel::editCompleted);
    QPointer<InlineTextEditOverlay> originalOverlay;
    if (createBeforeMove) {
        QTest::mouseDClick(&label, Qt::LeftButton);
        auto *overlay = firstHost->findChild<InlineTextEditOverlay *>();
        QVERIFY(overlay && overlay->isEditing());
        originalOverlay = overlay;
        auto *input = overlay->findChild<QLineEdit *>();
        QVERIFY(input);
        QTest::keyClick(input, Qt::Key_Escape);
        QVERIFY(!overlay->isEditing());
        QCOMPARE(commits.size(), 0);
    }

    if (!fallbackHost)
        label.setOverlayParent(nextHost);
    label.setParent(nextHost);
    label.setGeometry(10, 10, 180, 28);
    if (popupFirst)
        popupHost.hide();
    nextHost->show();
    nextHost->window()->activateWindow();
    label.show();
    QTest::mouseDClick(&label, Qt::LeftButton);
    auto *overlay = nextHost->findChild<InlineTextEditOverlay *>();
    QVERIFY(overlay && overlay->isEditing());
    QCOMPARE(overlay->parentWidget(), nextHost);
    if (originalOverlay)
        QCOMPARE(overlay, originalOverlay.data());
    auto *input = overlay->findChild<QLineEdit *>();
    QVERIFY(input);
    QTRY_VERIFY(input->hasFocus());
    QCOMPARE(overlay->pos(), nextHost->mapFromGlobal(label.mapToGlobal(QPoint())));
    input->setText(QStringLiteral("After migration"));
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(commits.size(), 1);
    QCOMPARE(label.text(), QStringLiteral("After migration"));
    QVERIFY(!overlay->isEditing());
}

void GuiComponentTests::inlineLabelSameHostKeepsEditingAndHostChangeCommitsOnce_data() {
    QTest::addColumn<bool>("popupFirst");
    QTest::newRow("inline-to-popup") << false;
    QTest::newRow("popup-to-inline") << true;
}

void GuiComponentTests::inlineLabelSameHostKeepsEditingAndHostChangeCommitsOnce() {
    QFETCH(bool, popupFirst);
    QWidget window;
    window.resize(500, 160);
    QWidget inlineHost(&window);
    inlineHost.setGeometry(0, 0, 450, 100);
    QWidget popupHost(&window, Qt::Popup);
    popupHost.resize(450, 100);
    auto *firstHost = popupFirst ? &popupHost : &inlineHost;
    auto *nextHost = popupFirst ? &inlineHost : &popupHost;
    InlineEditLabel label(firstHost);
    label.setGeometry(10, 10, 180, 28);
    label.setText(QStringLiteral("Original"));
    label.setOverlayParent(firstHost);
    window.show();
    firstHost->show();
    firstHost->window()->activateWindow();
    label.show();
    QSignalSpy commits(&label, &InlineEditLabel::editCompleted);
    QTest::mouseDClick(&label, Qt::LeftButton);
    auto *overlay = firstHost->findChild<InlineTextEditOverlay *>();
    QVERIFY(overlay && overlay->isEditing());
    auto *input = overlay->findChild<QLineEdit *>();
    QVERIFY(input);
    input->setText(QStringLiteral("Pending"));
    label.setOverlayParent(firstHost);
    // Explicit and fallback parents identify the same effective host.
    label.setOverlayParent(nullptr);
    QVERIFY(overlay->isEditing());
    QCOMPARE(commits.size(), 0);
    firstHost->resize(440, 110);
    label.resize(190, 28);
    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(firstHost->window(), &deactivate);
    QVERIFY(overlay->isEditing());
    QCOMPARE(commits.size(), 0);

    label.setOverlayParent(nextHost);
    QCOMPARE(commits.size(), 1);
    QCOMPARE(commits.first().first().toString(), QStringLiteral("Pending"));
    QCOMPARE(label.text(), QStringLiteral("Pending"));
    QVERIFY(!overlay->isEditing());
    QCOMPARE(overlay->parentWidget(), nextHost);
    label.finishEditing();
    label.setOverlayParent(nextHost);
    QCOMPARE(commits.size(), 1);
    label.setParent(nextHost);
    label.setGeometry(10, 10, 180, 28);
    if (popupFirst)
        popupHost.hide();
    nextHost->show();
    nextHost->window()->activateWindow();
    label.show();
    QTest::mouseDClick(&label, Qt::LeftButton);
    QVERIFY(overlay->isEditing());
    QCOMPARE(overlay->parentWidget(), nextHost);
    input->setText(QStringLiteral("Canceled"));
    QTest::keyClick(input, Qt::Key_Escape);
    QCOMPARE(commits.size(), 1);
    QCOMPARE(label.text(), QStringLiteral("Pending"));
}

void GuiComponentTests::inlineLabelPopupRoundTripKeepsContextMenuEditing() {
    QWidget window;
    window.resize(500, 160);
    QWidget popup(&window, Qt::Popup);
    popup.resize(450, 100);
    InlineEditLabel label(&popup);
    label.setGeometry(10, 10, 200, 28);
    label.setText(QStringLiteral("Original"));
    label.setOverlayParent(&popup);
    window.show();
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    popup.show();
    label.show();
    QCoreApplication::processEvents();
    QCOMPARE(QApplication::activePopupWidget(), &popup);
    QSignalSpy commits(&label, &InlineEditLabel::editCompleted);
    QTest::mouseDClick(&label, Qt::LeftButton);
    auto *overlay = popup.findChild<InlineTextEditOverlay *>();
    QVERIFY(overlay && overlay->isEditing());
    auto *input = overlay->findChild<QLineEdit *>();
    QVERIFY(input);
    QTRY_VERIFY(input->hasFocus());
    input->setText(QStringLiteral("Menu must not submit"));
    const auto pos = input->rect().center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, pos, input->mapToGlobal(pos));
    QApplication::sendEvent(input, &event);
    QPointer<Menu> menu = qobject_cast<Menu *>(QApplication::activePopupWidget());
    QVERIFY(menu);
    // Let the menu's queued platform activation arrive before simulating user input.
    QCoreApplication::processEvents();
    QCOMPARE(QApplication::activePopupWidget(), menu.data());
    const auto closeMenu = qScopeGuard([&] {
        if (menu)
            menu->close();
    });
    QVERIFY(popup.isVisible());
    QVERIFY(overlay->isEditing());
    QCOMPARE(commits.size(), 0);
    // Escape closes only the nested menu, not the toolbar popup or text editor.
    QTest::keyClick(menu, Qt::Key_Escape);
    QTRY_VERIFY(menu.isNull());
    QVERIFY(popup.isVisible());
    QTRY_VERIFY(input->hasFocus());
    QVERIFY(overlay->isEditing());
    QCOMPARE(commits.size(), 0);
    QTest::keyClick(input, Qt::Key_Escape);
    QCOMPARE(label.text(), QStringLiteral("Original"));
    QCOMPARE(commits.size(), 0);
    QVERIFY(popup.isVisible());
    QTest::mouseDClick(&label, Qt::LeftButton);
    QVERIFY(overlay->isEditing());
    input->setText(QStringLiteral("Committed"));
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(label.text(), QStringLiteral("Committed"));
    QCOMPARE(commits.size(), 1);
    QVERIFY(!overlay->isEditing());
    popup.hide();
    popup.show();
    QTest::mouseDClick(&label, Qt::LeftButton);
    QVERIFY(overlay->isEditing());
    input->setText(QStringLiteral("Commit on host hide"));
    popup.hide();
    QCOMPARE(label.text(), QStringLiteral("Commit on host hide"));
    QCOMPARE(commits.size(), 2);
}
