#include "tst_gui_components.h"

#include <lite/GUI/Controls/DragHandle.h>
#include <lite/GUI/Controls/SvsSeekbar.h>
#include <lite/GUI/Controls/SmoothScroller.h>
#include <lite/GUI/Controls/TouchClaimFilter.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/ComboPopupTouchFilter.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QLabel>
#include <QListWidget>
#include <QScroller>
#include <QScrollBar>
#include <QScrollArea>
#include <QTest>
#include <QTouchEvent>
#include <QVBoxLayout>
#include <QWidget>
#include <QAbstractItemView>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>

#include <memory>

namespace {

    // Records that a claimed widget actually received the replayed mouse press.
    class MouseButtonRecorder : public QObject {
    public:
        using QObject::QObject;

        bool sawPress = false;
        bool sawRelease = false;
        /// Source of the last press. A press this claim replayed is
        /// NotSynthesized; a press Qt itself derived from an unaccepted touch is
        /// SynthesizedByQt. That distinction is the only way to tell "the claim
        /// took this touch" from "the claim let it through and Qt handled it".
        Qt::MouseEventSource lastSource = Qt::MouseEventNotSynthesized;

        [[nodiscard]] bool sawReplayedPress() const {
            return sawPress && lastSource != Qt::MouseEventSynthesizedByQt;
        }

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override {
            if (event->type() == QEvent::MouseButtonPress) {
                sawPress = true;
                lastSource = static_cast<QMouseEvent *>(event)->source();
            }
            if (event->type() == QEvent::MouseButtonRelease)
                sawRelease = true;
            return QObject::eventFilter(watched, event);
        }
    };

    QWidget *makeTallContent(int height) {
        auto *content = new QWidget;
        content->setFixedSize(400, height);
        return content;
    }

    QPoint seekBarCenterInViewport(const SVS::SeekBar *seekBar, const QScrollArea *area) {
        return seekBar->mapTo(area->viewport(), seekBar->rect().center());
    }

    // Drags the primary touch point from \p from to \p to in viewport-local
    // coordinates, spacing the moves so QScroller can estimate a flick velocity.
    void touchDrag(QWidget *viewport, QPointingDevice *device, const QPoint &from, const QPoint &to,
                   int steps, QStringList *trace = nullptr) {
        QElapsedTimer elapsed;
        elapsed.start();
        const auto record = [&] {
            if (!trace)
                return;
            const auto *scroller = QScroller::scroller(viewport);
            trace->append(QStringLiteral("t=%1 state=%2 velocity=%3,%4")
                              .arg(elapsed.elapsed())
                              .arg(static_cast<int>(scroller->state()))
                              .arg(scroller->velocity().x())
                              .arg(scroller->velocity().y()));
        };
        QTest::touchEvent(viewport, device).press(0, from);
        QApplication::processEvents();
        record();
        // Carry the final motion in the release; an idle pause at the endpoint is not a flick.
        for (int i = 1; i < steps; ++i) {
            QTest::touchEvent(viewport, device).move(0, from + (to - from) * i / steps);
            QApplication::processEvents();
            record();
            QTest::qWait(15);
        }
        record();
        QTest::touchEvent(viewport, device).release(0, to);
        QApplication::processEvents();
        record();
    }

} // namespace

void GuiComponentTests::comboPopupTouchKeepsScrollingAndSelectionSeparate_data() {
    QTest::addColumn<bool>("customCombo");
    QTest::newRow("application-combo") << true;
    QTest::newRow("plain-log-combo") << false;
}

void GuiComponentTests::comboPopupTouchKeepsScrollingAndSelectionSeparate() {
    QFETCH(bool, customCombo);
    std::unique_ptr<QComboBox> owner;
    if (customCombo)
        owner = std::make_unique<ComboBox>();
    else
        owner = std::make_unique<QComboBox>();
    auto &combo = *owner;
    if (!customCombo)
        ComboPopupTouchFilter::install(&combo);
    auto font = combo.font();
    font.setPointSize(10);
    combo.setFont(font);
    for (int row = 0; row < 200; ++row)
        combo.addItem(QStringLiteral("Choice %1").arg(row));
    combo.setMaxVisibleItems(8);
    combo.resize(240, 32);
    combo.show();
    auto *view = combo.view();
    auto *viewport = view->viewport();
    auto *scroll = view->verticalScrollBar();
    QSignalSpy activated(&combo, &QComboBox::activated);
    QElapsedTimer elapsed;
    elapsed.start();
    QStringList trace;
    const auto close = qScopeGuard([&] {
        combo.hidePopup();
        QScroller::scroller(viewport)->stop();
    });
    const auto open = [&] {
        QTest::mouseClick(&combo, Qt::LeftButton);
        QTRY_VERIFY(view->isVisible() && viewport->height() > 80);
        QTRY_VERIFY(scroll->maximum() > 0);
        // Let Qt's opening-click guard expire before starting another touch.
        QTest::qWait(QApplication::doubleClickInterval());
    };
    const auto send = [&](QEvent::Type type, const QPoint &position, Qt::MouseEventSource source) {
        const auto button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
        const auto buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
        QMouseEvent event(type, QPointF(position), QPointF(position),
                          QPointF(viewport->mapToGlobal(position)), button, buttons, Qt::NoModifier,
                          source);
        event.setTimestamp(1000 + elapsed.elapsed());
        QApplication::sendEvent(viewport, &event);
        QApplication::processEvents();
        const auto *scroller = QScroller::scroller(viewport);
        trace.append(QStringLiteral("event=%1 source=%2 y=%3 state=%4 scroll=%5/%6 ppm=%7,%8")
                         .arg(type)
                         .arg(source)
                         .arg(position.y())
                         .arg(scroller->state())
                         .arg(scroll->value())
                         .arg(scroll->maximum())
                         .arg(scroller->pixelPerMeter().x())
                         .arg(scroller->pixelPerMeter().y()));
    };
    int tappedRow = -1;
    const auto tap = [&](int row) {
        const auto item = view->model()->index(row, 0);
        const auto visible = view->visualRect(item).intersected(viewport->rect());
        QVERIFY(!visible.isEmpty());
        trace.append(QStringLiteral("tap row=%1 y=%2 before=%3 scroll=%4")
                         .arg(row)
                         .arg(visible.center().y())
                         .arg(view->indexAt(visible.center()).row())
                         .arg(scroll->value()));
        send(QEvent::MouseButtonPress, visible.center(), Qt::MouseEventSynthesizedByQt);
        // Read the hit after the press has settled the list's scroll position.
        const auto touched = view->indexAt(visible.center());
        QVERIFY(touched.isValid());
        tappedRow = touched.row();
        trace.append(QStringLiteral("tap pressed under=%1 current=%2 scroll=%3")
                         .arg(view->indexAt(visible.center()).row())
                         .arg(view->currentIndex().row())
                         .arg(scroll->value()));
        send(QEvent::MouseButtonRelease, visible.center(), Qt::MouseEventSynthesizedByQt);
        QTRY_VERIFY2(combo.currentIndex() == tappedRow, qPrintable(trace.join('\n')));
        QTRY_VERIFY(!view->isVisible());
    };
    open();
    if (QTest::currentTestFailed())
        return;
    tap(3);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.first().first().toInt(), 3);
    activated.clear();

    open();
    if (QTest::currentTestFailed())
        return;
    const auto beforeScroll = scroll->value();
    const auto from = QPoint(viewport->width() / 2, viewport->height() - 12);
    const auto to = QPoint(from.x(), 12);
    send(QEvent::MouseButtonPress, from, Qt::MouseEventSynthesizedBySystem);
    for (int step = 1; step <= 8; ++step) {
        const auto position = from + (to - from) * step / 8;
        send(QEvent::MouseMove, position, Qt::MouseEventSynthesizedBySystem);
        if (step == 4)
            send(QEvent::MouseButtonPress, position, Qt::MouseEventSynthesizedByQt);
        QTest::qWait(15);
    }
    send(QEvent::MouseButtonRelease, to, Qt::MouseEventSynthesizedBySystem);
    QTRY_VERIFY2(scroll->value() > beforeScroll, qPrintable(trace.join('\n')));
    QVERIFY(view->isVisible());
    QCOMPARE(combo.currentIndex(), 3);
    QVERIFY(activated.isEmpty());
    QTRY_COMPARE(QScroller::scroller(viewport)->state(), QScroller::Inactive);
    const auto visibleItem = view->indexAt(viewport->rect().center());
    QVERIFY(visibleItem.isValid() && visibleItem.row() != combo.currentIndex());
    tap(visibleItem.row());
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.first().first().toInt(), tappedRow);
    activated.clear();

    open();
    if (QTest::currentTestFailed())
        return;
    const auto selected = combo.currentIndex();
    send(QEvent::MouseButtonPress, from, Qt::MouseEventSynthesizedBySystem);
    send(QEvent::MouseMove, to, Qt::MouseEventSynthesizedBySystem);
    QTest::keyClick(view, Qt::Key_Escape);
    QTRY_VERIFY(!view->isVisible());
    QCOMPARE(combo.currentIndex(), selected);
    QVERIFY(activated.isEmpty());
    open();
    if (QTest::currentTestFailed())
        return;
    const auto nextItem = view->indexAt(viewport->rect().center());
    QVERIFY(nextItem.isValid());
    tap(nextItem.row());
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.first().first().toInt(), tappedRow);
}

void GuiComponentTests::touchClaimsKeepControlsIndependentOfPageScrolling() {
    auto *touchDevice = QTest::createTouchDevice();

    // --- attachTo installs touch kinetic scrolling on the viewport ---
    QScrollArea scrollArea;
    scrollArea.setWidget(makeTallContent(1200));
    scrollArea.resize(400, 300);
    scrollArea.show();
    SmoothScroller scroller;
    scroller.attachTo(&scrollArea);
    QApplication::processEvents();

    QVERIFY2((scrollArea.viewport()->testAttribute(Qt::WA_AcceptTouchEvents)),
             "viewport accepts touch events after attach");
    // The offscreen platform reports a bogus physical DPI, which inflates the
    // meter-based DragStartDistance; neutralize it for deterministic tests.
    auto *scrollerHandle = QScroller::scroller(scrollArea.viewport());
    QScrollerProperties testProperties = scrollerHandle->scrollerProperties();
    testProperties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0);
    scrollerHandle->setScrollerProperties(testProperties);
    auto *viewport = scrollArea.viewport();
    auto *scrollBar = scrollArea.verticalScrollBar();
    touchDrag(viewport, touchDevice, QPoint(200, 260), QPoint(200, 40), 10);
    QVERIFY2((scrollBar->value() > 0), "touch drag scrolls the area");

    // --- drag-owning children are claimed automatically ---
    QScrollArea sliderArea;
    auto *sliderContent = makeTallContent(1200);
    const auto sliderLayout = new QVBoxLayout(sliderContent);
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    auto *seekBar = new SVS::SeekBar;
    seekBar->setRange(0.0, 100.0);
    seekBar->setValue(10.0);
    sliderLayout->addWidget(seekBar);
    sliderArea.setWidget(sliderContent);
    sliderArea.resize(400, 300);
    sliderArea.show();
    SmoothScroller sliderScroller;
    sliderScroller.attachTo(&sliderArea);
    QApplication::processEvents(); // runs the queued claim sweep
    QVERIFY2((seekBar->testAttribute(Qt::WA_AcceptTouchEvents)), "SVS::SeekBar child is claimed");

    const auto startValue = seekBar->value();
    // The seekbar sits vertically centered in the tall content; scroll it into
    // the viewport before touching it, then recompute its position.
    const auto center = [&] {
        const auto unscrolled = seekBarCenterInViewport(seekBar, &sliderArea);
        sliderArea.verticalScrollBar()->setValue(qMax(0, unscrolled.y() - 100));
        QApplication::processEvents();
        return seekBarCenterInViewport(seekBar, &sliderArea);
    }();
    const auto scrollBeforeDrag = sliderArea.verticalScrollBar()->value();
    touchDrag(sliderArea.viewport(), touchDevice, center, center + QPoint(120, 4), 6);
    QVERIFY2((seekBar->value() > startValue), "touch drag adjusts the claimed seekbar");
    QVERIFY2((sliderArea.verticalScrollBar()->value() == scrollBeforeDrag),
             "claimed seekbar does not scroll the page");

    // --- TouchKinetic::Disabled keeps the stock behavior ---
    QScrollArea plainArea;
    plainArea.setWidget(makeTallContent(1200));
    plainArea.resize(400, 300);
    plainArea.show();
    QApplication::processEvents();
    const auto acceptedTouchBeforeAttach =
        plainArea.viewport()->testAttribute(Qt::WA_AcceptTouchEvents);
    SmoothScroller plainScroller;
    plainScroller.attachTo(&plainArea, SmoothScroller::TouchKinetic::Disabled);
    QApplication::processEvents();
    QCOMPARE(plainArea.viewport()->testAttribute(Qt::WA_AcceptTouchEvents),
             acceptedTouchBeforeAttach);
    touchDrag(plainArea.viewport(), touchDevice, QPoint(200, 260), QPoint(200, 40), 10);
    QVERIFY2((plainArea.verticalScrollBar()->value() == 0),
             "opt-out area does not scroll from a touch drag");

    // --- explicit claim (drag handles) replays touch as mouse ---
    QLabel handle;
    handle.resize(28, 28);
    handle.show();
    QApplication::processEvents();
    MouseButtonRecorder recorder;
    handle.installEventFilter(&recorder);
    SmoothScroller::installClaim(&handle);
    QVERIFY2((handle.testAttribute(Qt::WA_AcceptTouchEvents)),
             "explicit claim enables touch delivery");
    QTest::touchEvent(&handle, touchDevice).press(0, QPoint(5, 5));
    QApplication::processEvents();
    QVERIFY2((recorder.sawPress), "claimed widget receives the replayed mouse press");
    QTest::touchEvent(&handle, touchDevice).release(0, QPoint(5, 5));
    QApplication::processEvents();

    // --- a system cancel notifies the target and still closes the stream ---
    // The system can take a claimed touch away mid-stream (QEvent::TouchCancel).
    // The replayed release must still go out, or the target's pressed state
    // sticks; but the target is told first, so it can drop what the release
    // would otherwise commit - a track reorder mid-drag, for one.
    QLabel cancelledTarget;
    cancelledTarget.resize(28, 28);
    cancelledTarget.show();
    QApplication::processEvents();
    MouseButtonRecorder cancelRecorder;
    cancelledTarget.installEventFilter(&cancelRecorder);
    bool cancelNotified = false;
    TouchClaimFilter::install(&cancelledTarget, {}, [&cancelNotified] { cancelNotified = true; });
    QTest::touchEvent(&cancelledTarget, touchDevice).press(0, QPoint(5, 5));
    QApplication::processEvents();
    QVERIFY2((cancelRecorder.sawReplayedPress()),
             "the claimed press is replayed before the cancel");

    // QTest has no touch cancel; deliver one the way the platform does, as a
    // hand-built QTouchEvent aimed at the claimed widget.
    QTouchEvent cancelEvent(QEvent::TouchCancel, touchDevice, Qt::NoModifier,
                            QList<QEventPoint>{QEventPoint(1, QEventPoint::State::Released,
                                                           QPointF(5, 5), QPointF(5, 5))});
    QApplication::sendEvent(&cancelledTarget, &cancelEvent);
    QApplication::processEvents();
    QVERIFY2((cancelNotified), "a system cancel notifies the target before the replayed release");
    QVERIFY2((cancelRecorder.sawRelease), "the cancel still closes the stream with a release");

    // A cancel with no claimed press behind it must not fire the notice.
    bool idleNotified = false;
    QLabel idleTarget;
    idleTarget.resize(28, 28);
    idleTarget.show();
    QApplication::processEvents();
    MouseButtonRecorder idleRecorder;
    idleTarget.installEventFilter(&idleRecorder);
    TouchClaimFilter::install(&idleTarget, {}, [&idleNotified] { idleNotified = true; });
    QTouchEvent strayCancel(QEvent::TouchCancel, touchDevice, Qt::NoModifier,
                            QList<QEventPoint>{QEventPoint(1, QEventPoint::State::Released,
                                                           QPointF(5, 5), QPointF(5, 5))});
    QApplication::sendEvent(&idleTarget, &strayCancel);
    QApplication::processEvents();
    QVERIFY2((!idleNotified && !idleRecorder.sawRelease),
             "a stray cancel with no claimed press notifies nothing and replays nothing");

    // --- claim re-installs after the content tree is rebuilt ---
    auto *replacement = makeTallContent(1200);
    const auto replacementLayout = new QVBoxLayout(replacement);
    replacementLayout->setContentsMargins(0, 0, 0, 0);
    auto *replacementBar = new SVS::SeekBar;
    replacementBar->setRange(0.0, 100.0);
    replacementLayout->addWidget(replacementBar);
    sliderArea.setWidget(replacement);
    QApplication::processEvents();
    QVERIFY2((replacementBar->testAttribute(Qt::WA_AcceptTouchEvents)),
             "claim sweep re-runs after setWidget rebuild");

    // --- a gated claim only takes the touches inside its hit test ---
    // This is what lets a list own just its reorder grip: a touch on the grip is
    // claimed (and reorders), a touch on the row falls through (and scrolls).
    QLabel gated;
    gated.resize(60, 28);
    gated.show();
    QApplication::processEvents();
    MouseButtonRecorder gatedRecorder;
    gated.installEventFilter(&gatedRecorder);
    SmoothScroller::installClaim(&gated, [](const QPointF &pos) { return pos.x() < 30.0; });

    QTest::touchEvent(&gated, touchDevice).press(0, QPoint(5, 14));
    QTest::touchEvent(&gated, touchDevice).release(0, QPoint(5, 14));
    QApplication::processEvents();
    QVERIFY2((gatedRecorder.sawReplayedPress()),
             "a touch inside the hit test is replayed by the claim");

    MouseButtonRecorder outsideRecorder;
    gated.installEventFilter(&outsideRecorder);
    QTest::touchEvent(&gated, touchDevice).press(0, QPoint(55, 14));
    QTest::touchEvent(&gated, touchDevice).release(0, QPoint(55, 14));
    QApplication::processEvents();
    QVERIFY2(
        (outsideRecorder.sawPress && outsideRecorder.lastSource == Qt::MouseEventSynthesizedByQt),
        "a touch outside the hit test is left to Qt's own synthesis");

    // --- a disabled handle gives its claim back ---
    DragHandle disabledHandle;
    disabledHandle.show();
    QApplication::processEvents();
    MouseButtonRecorder disabledRecorder;
    disabledHandle.installEventFilter(&disabledRecorder);
    disabledHandle.setDragEnabled(false);
    QTest::touchEvent(&disabledHandle, touchDevice).press(0, QPoint(5, 5));
    QApplication::processEvents();
    QVERIFY2((!disabledRecorder.sawReplayedPress()), "a disabled handle does not claim the touch");

    // --- a claim on an inner viewport pins every ancestor scroller ---
    // The claim target is itself a viewport here, and isAncestorOf() counts a
    // widget as its own ancestor: the walk up from the target matches the inner
    // scroll area first, so pinning only the nearest scroller leaves the page
    // that actually scrolls engaged. Measured on device as "the row reorders and
    // the page scrolls at the same time".
    //
    // The observable is the ancestor scroller's state, not the scrollbar: the
    // gesture manager feeds every grabbed scroller up the ancestor chain, and it
    // does so before any event filter runs, so the page is engaged by a touch
    // aimed at the inner widget whether or not that touch is delivered on.
    //
    // The inner area is a QListWidget rather than a plain QScrollArea so that
    // nothing sits over its viewport: a QScrollArea paints its content widget on
    // top of the viewport, so the finger would never reach a claim installed
    // there. This mirrors the real case, a list claiming its own viewport.
    QScrollArea page;
    auto *pageContent = makeTallContent(1200);
    auto *inner = new QListWidget(pageContent);
    inner->setGeometry(0, 0, 380, 200);
    for (int i = 0; i < 40; ++i)
        inner->addItem(QStringLiteral("row %1").arg(i));
    page.setWidget(pageContent);
    page.resize(400, 300);
    page.show();
    SmoothScroller pageScroller;
    pageScroller.attachTo(&page);
    QApplication::processEvents();
    auto *pageHandle = QScroller::scroller(page.viewport());
    auto pageProperties = pageHandle->scrollerProperties();
    pageProperties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0);
    pageHandle->setScrollerProperties(pageProperties);

    // QScroller's recognizer ignores a press while another scroller is active
    // ("active scrollers always have priority"), and the flick in the first
    // section can still be gliding; settle it so this section measures what it
    // means to.
    for (auto *active : QScroller::activeScrollers())
        active->stop();
    QApplication::processEvents();

    bool claimOwnsTouches = false;
    SmoothScroller::installClaim(inner->viewport(),
                                 [&claimOwnsTouches](const QPointF &) { return claimOwnsTouches; });

    // Control: while the inner widget does not own the touch, the page scroller
    // engages as usual - so the assertion below cannot pass for lack of a client.
    QTest::touchEvent(inner->viewport(), touchDevice).press(0, QPoint(190, 180));
    QApplication::processEvents();
    QVERIFY2((pageHandle->state() != QScroller::Inactive),
             "an unclaimed touch engages the page's scroller");
    QTest::touchEvent(inner->viewport(), touchDevice).release(0, QPoint(190, 180));
    QApplication::processEvents();
    pageHandle->stop();
    page.verticalScrollBar()->setValue(0);

    // Owned: every ancestor scroller has to be pinned back to Inactive, or the
    // page keeps dragging while the claimed widget is dragged.
    claimOwnsTouches = true;
    QTest::touchEvent(inner->viewport(), touchDevice).press(0, QPoint(190, 180));
    QApplication::processEvents();
    QVERIFY2((pageHandle->state() == QScroller::Inactive),
             "a claim on an inner viewport pins the page's scroller back to Inactive");
    for (int i = 1; i <= 6; ++i) {
        QTest::touchEvent(inner->viewport(), touchDevice).move(0, QPoint(190, 180 - i * 20));
        QApplication::processEvents();
    }
    QVERIFY2((pageHandle->state() == QScroller::Inactive),
             "the page's scroller stays out of the claimed drag");
    QVERIFY2((page.verticalScrollBar()->value() == 0),
             "a claim on an inner viewport keeps the page from scrolling too");
    QTest::touchEvent(inner->viewport(), touchDevice).release(0, QPoint(190, 60));
    QApplication::processEvents();
}

void GuiComponentTests::touchFlickContinuesAfterRelease() {
    auto *touchDevice = QTest::createTouchDevice();

    // --- attachTo installs touch kinetic scrolling on the viewport ---
    QScrollArea scrollArea;
    scrollArea.setWidget(makeTallContent(1200));
    scrollArea.resize(400, 300);
    scrollArea.show();
    SmoothScroller scroller;
    scroller.attachTo(&scrollArea);
    QApplication::processEvents();

    QVERIFY2((scrollArea.viewport()->testAttribute(Qt::WA_AcceptTouchEvents)),
             "viewport accepts touch events after attach");
    // The offscreen platform reports a bogus physical DPI, which inflates the
    // meter-based DragStartDistance; neutralize it for deterministic tests.
    auto *scrollerHandle = QScroller::scroller(scrollArea.viewport());
    QScrollerProperties testProperties = scrollerHandle->scrollerProperties();
    testProperties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0);
    scrollerHandle->setScrollerProperties(testProperties);
    const auto ppm = scrollerHandle->pixelPerMeter();
    if (!qIsFinite(ppm.x()) || !qIsFinite(ppm.y()) || ppm.x() <= 0 || ppm.y() <= 0)
        QSKIP("The platform does not report usable physical metrics for QScroller inertia");
    auto *viewport = scrollArea.viewport();
    QStringList trace;
    touchDrag(viewport, touchDevice, QPoint(200, 260), QPoint(200, 40), 10, &trace);
    QVERIFY2(
        scrollerHandle->state() == QScroller::Scrolling,
        qPrintable(
            QStringLiteral(
                "state=%1 ppm=%2,%3 velocity=%4,%5 minimum=%6 value=%7 maximum=%8 trace=%9")
                .arg(static_cast<int>(scrollerHandle->state()))
                .arg(ppm.x())
                .arg(ppm.y())
                .arg(scrollerHandle->velocity().x())
                .arg(scrollerHandle->velocity().y())
                .arg(testProperties.scrollMetric(QScrollerProperties::MinimumVelocity).toReal())
                .arg(scrollArea.verticalScrollBar()->value())
                .arg(scrollArea.verticalScrollBar()->maximum())
                .arg(trace.join(QStringLiteral("; ")))));
    auto *scrollBar = scrollArea.verticalScrollBar();
    const auto valueAtRelease = scrollBar->value();
    QTRY_VERIFY(scrollBar->value() > valueAtRelease);
}
