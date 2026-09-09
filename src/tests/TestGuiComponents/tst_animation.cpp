#include "tst_gui_components.h"

#include <lite/GUI/Animation/ElasticAnimator.h>

#include <QtTest/QTest>
#include <QSignalSpy>

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
