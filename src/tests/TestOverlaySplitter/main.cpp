#include <lite/GUI/Controls/OverlaySplitter.h>

#include <QtTest/QTest>
#include <QMouseEvent>
#include <QPointer>
#include <QResizeEvent>

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

class OverlaySplitterTests final : public QObject {
    Q_OBJECT

private slots:

    void visibilityAndCollapsedPane() {
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

    void reparentAndDestructionKeepGripOwnership() {
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

    void dragGrip_data() {
        QTest::addColumn<int>("orientation");
        QTest::newRow("horizontal") << int(Qt::Horizontal);
        QTest::newRow("vertical") << int(Qt::Vertical);
    }

    void dragGrip() {
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
};

QTEST_MAIN(OverlaySplitterTests)
#include "main.moc"
