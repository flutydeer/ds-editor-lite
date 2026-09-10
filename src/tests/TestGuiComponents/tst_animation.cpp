#include "tst_gui_components.h"

#include <lite/GUI/Animation/ElasticAnimator.h>
#include <lite/GUI/Controls/Toast.h>

#include <QtTest/QTest>
#include <QLabel>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>

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
