#include "tst_native_desktop.h"
#include "../TestSupport/GuiAppFixture.h"
#include "Automation/CoreRuntime.h"
#include "UI/Window/MainWindow.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/Common/TabPanelTitleBar.h"

#include <lite/GUI/Controls/OverlaySplitter.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QtTest/QTest>
#include <QMouseEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QScopeGuard>

namespace {
    class ResizeProbe final : public QWidget {
    public:
        int zeroHeightResizeCount = 0;

    protected:
        void resizeEvent(QResizeEvent *event) override {
            QWidget::resizeEvent(event);
            if (event->size().height() == 0)
                ++zeroHeightResizeCount;
        }
    };
}

void NativeDesktopTests::visibilityAndCollapsedPane() {
    QWidget host;
    host.resize(640, 480);
    auto *splitter = new OverlaySplitter(Qt::Vertical, &host);
    splitter->setGeometry(host.rect());
    splitter->addWidget(new QWidget);
    auto *pane = new ResizeProbe;
    splitter->addWidget(pane);
    host.show();
    QTRY_VERIFY(host.findChild<SplitterOverlayGrip *>());
    auto *grip = host.findChild<SplitterOverlayGrip *>();
    QTRY_VERIFY(grip->isVisible());
    splitter->setSizes({1, 0});
    QTRY_COMPARE(pane->height(), 0);
    QVERIFY(pane->zeroHeightResizeCount > 0);
    splitter->hide();
    QTRY_VERIFY(!grip->isVisible());
    splitter->show();
    QTRY_VERIFY(grip->isVisible());
}

void NativeDesktopTests::reparentAndDestructionKeepGripOwnership() {
    QWidget firstHost;
    firstHost.resize(640, 480);
    auto *splitter = new OverlaySplitter(Qt::Vertical, &firstHost);
    splitter->setGeometry(firstHost.rect());
    splitter->addWidget(new QWidget);
    splitter->addWidget(new QWidget);
    firstHost.show();
    QTRY_VERIFY(firstHost.findChild<SplitterOverlayGrip *>());
    QPointer<SplitterOverlayGrip> grip = firstHost.findChild<SplitterOverlayGrip *>();

    QWidget secondHost;
    secondHost.resize(640, 480);
    secondHost.show();
    splitter->setParent(&secondHost);
    splitter->setGeometry(secondHost.rect());
    splitter->show();
    QTRY_VERIFY(grip && grip->isVisible());
    QVERIFY(!firstHost.findChild<SplitterOverlayGrip *>());
    QCOMPARE(secondHost.findChild<SplitterOverlayGrip *>(), grip.data());
    delete splitter;
    QVERIFY(grip.isNull());
}

void NativeDesktopTests::customWindowButtonsKeepTheDetachedPanelAndDocument() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("Custom window frames require a native window backend");
    const auto exerciseButtons = [](QWidget &host, QWidget *minimize, QWidget *maximize) {
        QVERIFY(minimize && maximize);
        QTest::mouseClick(maximize, Qt::LeftButton);
        QTRY_VERIFY(host.isMaximized());
        QTest::mouseClick(maximize, Qt::LeftButton);
        QTRY_VERIFY(!host.isMaximized());
        QTest::mouseClick(minimize, Qt::LeftButton);
        QTRY_VERIFY(host.isMinimized());
        host.showNormal();
        host.activateWindow();
        QTRY_VERIFY(!host.isMinimized() && host.isActiveWindow());
    };
    GuiDocumentFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    const auto nativeFrame = appOptions->appearance()->useNativeFrame;
    const auto detachEnabled = appOptions->developer()->enablePanelDetach;
    const auto restoreOptions = qScopeGuard([&] {
        appOptions->appearance()->useNativeFrame = nativeFrame;
        appOptions->developer()->enablePanelDetach = detachEnabled;
    });
    appOptions->appearance()->useNativeFrame = false;
    appOptions->developer()->enablePanelDetach = true;
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    exerciseButtons(window, window.findChild<QWidget *>("MinimizeButton"),
                    window.findChild<QWidget *>("MaximizeButton"));
    if (QTest::currentTestFailed())
        return;
    QVERIFY(window.showBottomPanelPage(QStringLiteral("MixConsole")));
    auto *bottom = window.findChild<BottomPanelView *>();
    QVERIFY(bottom);
    auto *splitter = qobject_cast<QSplitter *>(bottom->parentWidget());
    QVERIFY(splitter);
    QCoreApplication::processEvents();
    const auto reattach = qScopeGuard([&] {
        if (bottom->isWindow())
            bottom->close();
    });
    const auto before = fixture.context->m_coreRuntime->documentVersion();
    const auto beforeModel = fixture.context->m_appModel->serialize();
    const auto sizes = splitter->sizes();
    auto *detach = bottom->titleBar()->findChild<Button *>("btnPanelDetach");
    QVERIFY(detach && detach->isVisible());
    QTest::mouseClick(detach, Qt::LeftButton);
    QTRY_VERIFY(bottom->isWindow() && bottom->isVisible());
    bottom->activateWindow();
    QTRY_VERIFY(bottom->isActiveWindow());
    auto *title = bottom->titleBar();
    QVERIFY(title->maximizeButton() && title->minimizeButton() && title->closeButton());
    exerciseButtons(*bottom, title->minimizeButton(), title->maximizeButton());
    if (QTest::currentTestFailed())
        return;
    QTest::mouseClick(title->closeButton(), Qt::LeftButton);
    QTRY_VERIFY(!bottom->isWindow() && bottom->isVisible());
    QCOMPARE(bottom->parentWidget(), splitter);
    QTRY_COMPARE(splitter->sizes(), sizes);
    QCOMPARE(bottom->currentPageId(), QStringLiteral("MixConsole"));
    QCOMPARE(fixture.context->m_coreRuntime->documentVersion(), before);
    QCOMPARE(fixture.context->m_appModel->serialize(), beforeModel);
    QVERIFY(!historyManager->canUndo());
}

void NativeDesktopTests::dragGrip_data() {
    QTest::addColumn<int>("orientation");
    QTest::newRow("horizontal") << int(Qt::Horizontal);
    QTest::newRow("vertical") << int(Qt::Vertical);
}

void NativeDesktopTests::dragGrip() {
    QFETCH(int, orientation);
    QWidget host;
    host.resize(640, 480);
    auto *splitter = new OverlaySplitter(Qt::Orientation(orientation), &host);
    splitter->setGeometry(host.rect());
    splitter->addWidget(new QWidget);
    splitter->addWidget(new QWidget);
    host.show();
    QTRY_VERIFY(host.findChild<SplitterOverlayGrip *>());
    auto *grip = host.findChild<SplitterOverlayGrip *>();
    QTRY_VERIFY(grip->isVisible());
    const auto before = splitter->sizes();
    QCOMPARE(before.size(), 2);
    const auto press = grip->rect().center();
    const auto global = grip->mapToGlobal(press);
    const auto movement = orientation == Qt::Horizontal ? QPoint(40, 0) : QPoint(0, 40);
    QTest::mousePress(grip, Qt::LeftButton, Qt::NoModifier, press);
    QMouseEvent move(QEvent::MouseMove, QPointF(press + movement), QPointF(global + movement),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(grip, &move);
    QTest::mouseRelease(grip, Qt::LeftButton);
    QTRY_VERIFY(splitter->sizes().first() > before.first());
    QVERIFY(splitter->sizes().last() < before.last());
    const auto released = splitter->sizes();
    QApplication::sendEvent(grip, &move);
    QCOMPARE(splitter->sizes(), released);
}
