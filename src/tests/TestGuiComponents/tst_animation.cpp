#include "tst_gui_components.h"

#include <lite/GUI/Animation/ElasticAnimator.h>
#include <lite/GUI/Controls/Toast.h>
#include <lite/GUI/Controls/LevelMeterViewModel.h>
#include <lite/GUI/Controls/ToolTip.h>
#include <lite/GUI/Controls/ToolTipFilter.h>

#include <QtTest/QTest>
#include <QLabel>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QPushButton>
#include <QEnterEvent>

#include <memory>

void GuiComponentTests::reachesTarget_data() {
    QTest::addColumn<QPointF>("target");
    QTest::newRow("positive") << QPointF(100, 50);
    QTest::newRow("negative") << QPointF(-80, -20);
}

void GuiComponentTests::reachesTarget() {
    QFETCH(QPointF, target);
    ElasticAnimator animator;
    animator.setSmoothness(0.1);
    animator.setResponsiveness(0.2);
    QSignalSpy updates(&animator, &ElasticAnimator::positionUpdated);
    animator.setTarget(target);
    QTRY_COMPARE_WITH_TIMEOUT(animator.position(), target, 5000);
    QCOMPARE(animator.velocity(), QPointF());
    QVERIFY(!updates.isEmpty());
    QCOMPARE(updates.last().at(0).toPointF(), target);
}

void GuiComponentTests::replacingTargetChangesDestination() {
    ElasticAnimator animator;
    animator.setSmoothness(0.1);
    animator.setResponsiveness(0.2);
    animator.setTarget(QPointF(100, 50));
    QTRY_VERIFY(animator.position() != QPointF());
    const QPointF replacement(-20, 60);
    animator.setTarget(replacement);
    QTRY_COMPARE_WITH_TIMEOUT(animator.position(), replacement, 5000);
    QCOMPARE(animator.velocity(), QPointF());
}

void GuiComponentTests::meterPeaksHoldDecayAndKeepClippingLatched() {
    LevelMeterViewModel model(20, 20);
    QSignalSpy peakText(&model, &LevelMeterViewModel::peakValueChanged);
    model.setLevels(0, -6);
    QVERIFY(!model.clippedL() && !model.clippedR());
    QCOMPARE(model.levelL(), 1.0);
    QCOMPARE(model.peakValue(), 0.0);
    model.setLevels(3, 6);
    QVERIFY(model.clippedL() && model.clippedR());
    QVERIFY(model.levelR() > model.levelL() && model.levelL() > 1.0);
    QCOMPARE(model.peakValue(), 6.0);
    model.setLevels(-70, -70);
    QCOMPARE(model.levelL(), 0.0);
    QCOMPARE(model.levelR(), 0.0);
    QCOMPARE(model.peakValue(), 6.0);
    QTRY_COMPARE(model.displayedPeakL(), 0.0);
    QTRY_COMPARE(model.displayedPeakR(), 0.0);
    QCOMPARE(model.peakValue(), -70.0);
    QVERIFY(!peakText.isEmpty());
    QCOMPARE(peakText.last().at(0).toDouble(), -70.0);
    QVERIFY(model.clippedL() && model.clippedR());
    model.resetClip();
    QVERIFY(!model.clippedL() && !model.clippedR());
}

void GuiComponentTests::aNewMeterPeakInterruptsDecay() {
    LevelMeterViewModel model(1, 60000);
    model.setLevels(0, -70);
    model.setLevels(-70, -70);
    QTRY_VERIFY(model.displayedPeakL() > 0.0 && model.displayedPeakL() < 1.0);
    model.setLevels(6, -70);
    QVERIFY(model.displayedPeakL() > 1.0);
    QCOMPARE(model.peakValue(), 6.0);
    model.setLevels(-70, -70);
    QCOMPARE(model.peakValue(), 6.0);
}

void GuiComponentTests::toastContextLifetime_data() {
    QTest::addColumn<bool>("replaceContext");
    QTest::newRow("replace-context") << true;
    QTest::newRow("destroy-owner") << false;
}

void GuiComponentTests::toastContextLifetime() {
    QFETCH(bool, replaceContext);
    auto *toast = Toast::instance();
    const auto animationEnabled = toast->animationEnabled();
    const auto cleanup = qScopeGuard([&] {
        Toast::setGlobalContext(nullptr);
        toast->setAnimationEnabled(animationEnabled);
    });
    toast->setAnimationEnabled(false);
    auto owner = std::make_unique<QWidget>();
    QWidget nextOwner;
    Toast::setGlobalContext(owner.get());
    Toast::show(QStringLiteral("Current owner message"));
    Toast::show(QStringLiteral("Queued owner message"));
    const QPointer<ToastWidget> original = owner->findChild<ToastWidget *>();
    QVERIFY(original);
    QVERIFY(original->isVisible());
    if (replaceContext) {
        Toast::setGlobalContext(&nextOwner);
        QVERIFY(!original);
    }
    owner.reset();
    QVERIFY(!original);
    // Deliver the same callback as an expired timer without waiting on wall-clock time.
    QVERIFY(QMetaObject::invokeMethod(toast, "hideToast", Qt::DirectConnection));
    Toast::setGlobalContext(&nextOwner);
    Toast::show(QStringLiteral("New owner message"));
    Toast::show(QStringLiteral("Next queued message"));
    const QPointer<ToastWidget> first = nextOwner.findChild<ToastWidget *>();
    QVERIFY(first);
    QCOMPARE(first->findChild<QLabel *>("toastMessage")->text(),
             QStringLiteral("New owner message"));
    QVERIFY(QMetaObject::invokeMethod(toast, "hideToast", Qt::DirectConnection));
    QVERIFY(!first);
    const QPointer<ToastWidget> second = nextOwner.findChild<ToastWidget *>();
    QVERIFY(second);
    QCOMPARE(second->findChild<QLabel *>("toastMessage")->text(),
             QStringLiteral("Next queued message"));
    QVERIFY(QMetaObject::invokeMethod(toast, "hideToast", Qt::DirectConnection));
    QVERIFY(!second);
    QVERIFY(nextOwner.findChildren<ToastWidget *>().isEmpty());
}

void GuiComponentTests::tooltipHoverRestoresUpdatedContent() {
    QPushButton button(QStringLiteral("Action"));
    button.setToolTip(QStringLiteral("Action tooltip"));
    button.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    ToolTipFilter filter(&button, 0, false, false);
    button.installEventFilter(&filter);
    filter.setMessage({QStringLiteral("First description"), QStringLiteral("Second description")});
    button.show();
    const auto enter = [&] {
        const QPointF position = button.rect().center();
        QEnterEvent event(position, position, button.mapToGlobal(position.toPoint()));
        QApplication::sendEvent(&button, &event);
    };
    enter();
    auto tip = QPointer(button.findChild<ToolTip *>());
    QVERIFY(tip);
    QTRY_VERIFY(tip->isVisible());
    QCOMPARE(tip->findChild<QLabel *>("toolTipTitle")->text(), button.toolTip());
    QCOMPARE(tip->findChild<QLabel *>("toolTipShortcutKey")->text(),
             button.shortcut().toString());
    QList<QPointer<QLabel>> oldLabels;
    for (auto *label : tip->findChildren<QLabel *>("toolTipMessage"))
        oldLabels.append(label);
    QCOMPARE(oldLabels.size(), 2);
    filter.setMessage({QStringLiteral("Updated description")});
    const auto updated = tip->findChildren<QLabel *>("toolTipMessage");
    QCOMPARE(updated.size(), 1);
    QCOMPARE(updated.first()->text(), QStringLiteral("Updated description"));
    for (const auto &label : oldLabels)
        QVERIFY(!label || tip->isAncestorOf(label));

    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&button, &leave);
    QTRY_VERIFY(tip.isNull());
    for (const auto &label : oldLabels)
        QVERIFY(label.isNull());
    button.setToolTip(QStringLiteral("Changed action"));
    filter.setMessage({QStringLiteral("Description after reopening")});
    enter();
    QTRY_VERIFY(button.findChild<ToolTip *>());
    tip = button.findChild<ToolTip *>();
    QTRY_VERIFY(tip->isVisible());
    QCOMPARE(tip->findChild<QLabel *>("toolTipTitle")->text(), button.toolTip());
    QCOMPARE(tip->findChild<QLabel *>("toolTipShortcutKey")->text(),
             button.shortcut().toString());
    QCOMPARE(tip->findChild<QLabel *>("toolTipMessage")->text(),
             QStringLiteral("Description after reopening"));
    button.hide();
    QTRY_VERIFY(tip.isNull());
}
