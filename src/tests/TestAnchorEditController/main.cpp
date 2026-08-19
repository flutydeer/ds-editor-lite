#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditController.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditUtils.h"

#include <lite/ProjectModel/AppModel/Curve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

namespace {
    int failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    AnchorCurve *makeCurve(std::initializer_list<QPoint> points) {
        auto *curve = new AnchorCurve;
        for (const auto &point : points)
            curve->insertNode(new AnchorNode(point.x(), point.y()));
        if (!curve->nodes().toList().isEmpty())
            curve->nodes().toList().last()->setInterpMode(AnchorNode::None);
        return curve;
    }

    AnchorEditor::CoordinateMapper mapper() {
        return {
            [](const double x) { return qRound(x / 10.0); },
            [](const int tick) { return tick * 10.0; },
            [](const double y) { return qBound(0, qRound((1000.0 - y) / 10.0), 100); },
            [](const int value) { return 1000.0 - qBound(0, value, 100) * 10.0; },
        };
    }

    void testLoadOwnsCopies() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        auto *source = makeCurve({
            {10, 20},
            {20, 40}
        });
        controller.loadFromModel({source});
        source->nodes().toList().first()->setValue(99);
        expect(controller.curves().size() == 1, "load must retain one curve");
        expect(controller.curves().first() != source, "load must deep-copy curves");
        expect(controller.curves().first()->nodes().toList().first()->value() == 20,
               "source mutation must not affect controller");
        delete source;
    }

    void testCreateAndPublishingReentry() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        int begins = 0;
        int commits = 0;
        controller.setHostCallbacks({
            [&] {
                ++begins;
                return true;
            },
            [&](const QList<AnchorCurve *> &curves) {
                ++commits;
                controller.loadFromModel({});
                expect(!curves.isEmpty(), "publish must receive working curves");
            },
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        controller.setEditActive(true);
        controller.doubleClickAt({100, 800}, Qt::LeftButton);
        expect(begins == 1 && commits == 1, "create must be one transaction");
        expect(controller.curves().size() == 1, "reentrant load must be ignored while publishing");
        if (controller.curves().isEmpty())
            return;
        expect(controller.curves().first()->nodes().toList().first()->pos() == 10,
               "created anchor tick must use mapper");
    }

    void testCreateClearsOverlappingPreview() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        controller.setHostCallbacks({
            [] { return true; },
            [](const QList<AnchorCurve *> &) {},
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        controller.setEditActive(true);
        controller.doubleClickAt({100, 800}, Qt::LeftButton);
        controller.hoverMoveAt({200, 600});
        expect(controller.state().showPreview, "editing hover must expose a preview node");

        controller.doubleClickAt({200, 600}, Qt::LeftButton);
        expect(controller.curves().first()->nodes().toList().size() == 2,
               "preview position must accept a second anchor");
        expect(!controller.state().showPreview && controller.state().previewCurve == nullptr,
               "creating a real anchor must clear its overlapping preview");
        expect(controller.state().hoveredNode != nullptr &&
                   controller.state().hoveredNode->pos() == 20,
               "the created anchor must replace the preview as the hovered node");
    }

    void testDragCancelRestoresSnapshot() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        auto *source = makeCurve({
            {10, 20},
            {20, 40}
        });
        int discarded = 0;
        controller.setHostCallbacks({
            [] { return true; },
            [](const QList<AnchorCurve *> &) {},
            [&](const AnchorEditor::EditFinishReason reason) {
                if (reason == AnchorEditor::EditFinishReason::Discard)
                    ++discarded;
            },
            [] {},
        });
        controller.loadFromModel({source});
        controller.setEditActive(true);
        controller.pressAt({100, 800}, Qt::LeftButton);
        controller.moveAt({130, 700}, Qt::LeftButton);
        if (controller.curves().isEmpty() ||
            controller.curves().first()->nodes().toList().isEmpty()) {
            expect(false, "drag must retain its working node");
            delete source;
            return;
        }
        expect(controller.curves().first()->nodes().toList().first()->pos() == 13,
               "drag must update working curve");
        controller.cancel();
        expect(discarded == 1, "cancelled drag must discard its transaction");
        expect(controller.curves().first()->nodes().toList().first()->pos() == 10,
               "cancelled drag must restore original tick");
        expect(controller.curves().first()->nodes().toList().first()->value() == 20,
               "cancelled drag must restore original value");
        delete source;
    }

    void testSelectionDeleteAndInterpolation() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        auto *source = makeCurve({
            {10, 20},
            {20, 40},
            {30, 60}
        });
        int publishes = 0;
        controller.setHostCallbacks({
            [] { return true; },
            [&](const QList<AnchorCurve *> &) { ++publishes; },
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        controller.loadFromModel({source});
        controller.setEditActive(true);
        controller.pressAt({200, 600}, Qt::LeftButton);
        controller.setSelectedInterpolation(AnchorNode::Linear);
        if (controller.curves().isEmpty() ||
            controller.curves().first()->nodes().toList().size() < 2) {
            expect(false, "selection test requires loaded nodes");
            delete source;
            return;
        }
        expect(controller.curves().first()->nodes().toList().at(1)->interpMode() ==
                   AnchorNode::Linear,
               "non-terminal interpolation must be editable");
        controller.deleteSelectedNodes();
        expect(controller.curves().first()->nodes().toList().size() == 2,
               "delete must remove selected node");
        expect(publishes == 2, "interpolation and delete must publish separately");
        delete source;
    }

    void testKeyboardCommands() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        auto *source = makeCurve({
            {10, 20},
            {20, 40},
            {30, 60}
        });
        controller.setHostCallbacks({
            [] { return true; },
            [](const QList<AnchorCurve *> &) {},
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        controller.loadFromModel({source});
        controller.setEditActive(true);

        expect(AnchorEditor::AnchorEditController::handlesKey(Qt::Key_Backspace),
               "Backspace must be an anchor edit key");
        expect(AnchorEditor::AnchorEditController::handlesKey(Qt::Key_Delete),
               "Delete must be an anchor edit key");
        expect(AnchorEditor::AnchorEditController::handlesKey(Qt::Key_Escape),
               "Escape must be an anchor edit key");
        expect(!AnchorEditor::AnchorEditController::handlesKey(Qt::Key_A),
               "unrelated keys must not be anchor edit keys");

        controller.pressAt({200, 600}, Qt::LeftButton);
        expect(controller.handleKeyPress(Qt::Key_Backspace), "Backspace must be handled");
        expect(controller.curves().first()->nodes().toList().size() == 2,
               "Backspace must delete the selected anchor");

        controller.loadFromModel({source});
        controller.pressAt({200, 600}, Qt::LeftButton);
        expect(controller.handleKeyPress(Qt::Key_Delete), "Delete must be handled");
        expect(controller.curves().first()->nodes().toList().size() == 2,
               "Delete must delete the selected anchor");

        controller.loadFromModel({source});
        controller.pressAt({200, 600}, Qt::LeftButton);
        expect(controller.handleKeyPress(Qt::Key_Escape), "Escape must be handled");
        expect(!controller.state().editing && controller.state().selectedNodes.isEmpty(),
               "Escape must leave anchor editing");
        expect(!controller.handleKeyPress(Qt::Key_A), "unrelated keys must remain unhandled");
        delete source;
    }

    void testCompositionPreservesOtherCurveKind() {
        auto *draw = new DrawCurve;
        draw->setLocalStart(0);
        draw->setValues({1, 2, 3});
        auto *anchor = makeCurve({
            {10, 20},
            {20, 40}
        });
        QList<Curve *> existing{draw, anchor};

        auto *replacementAnchor = makeCurve({
            {30, 60},
            {40, 80}
        });
        auto anchorResult = AnchorEditor::replaceAnchors(existing, {replacementAnchor});
        expect(anchorResult.size() == 2 && anchorResult.first()->type() == Curve::Draw &&
                   anchorResult.last()->type() == Curve::Anchor,
               "anchor replacement must preserve draw curves and ordering");

        auto *replacementDraw = new DrawCurve(*draw);
        auto drawResult = AnchorEditor::replaceDrawCurves(existing, {replacementDraw});
        expect(drawResult.size() == 2 && drawResult.first()->type() == Curve::Draw &&
                   drawResult.last()->type() == Curve::Anchor,
               "draw replacement must preserve anchor curves and ordering");

        qDeleteAll(anchorResult);
        qDeleteAll(drawResult);
        delete replacementDraw;
        delete replacementAnchor;
        qDeleteAll(existing);
    }

    void testCompositionRejectsSinglePointDrawCurves() {
        auto *anchor = makeCurve({
            {10, 20},
            {20, 40}
        });
        auto *singlePoint = new DrawCurve;
        singlePoint->setLocalStart(30);
        singlePoint->setValues({60});
        auto *draw = new DrawCurve;
        draw->setLocalStart(40);
        draw->setValues({70, 80});

        auto result = AnchorEditor::replaceDrawCurves({anchor}, {singlePoint, draw});
        expect(result.size() == 2 && result.first()->type() == Curve::Draw &&
                   static_cast<DrawCurve *>(result.first())->values() == QList<int>({70, 80}) &&
                   result.last()->type() == Curve::Anchor,
               "draw replacement must reject single-point curves and preserve valid curves");

        qDeleteAll(result);
        delete draw;
        delete singlePoint;
        delete anchor;
    }

    void testSelectionAndLastNodeMenu() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        auto *source = makeCurve({
            {10, 20},
            {20, 40},
            {30, 60}
        });
        controller.loadFromModel({source});
        controller.setEditActive(true);

        controller.pressAt({90, 810}, Qt::LeftButton);
        controller.moveAt({210, 590}, Qt::LeftButton);
        controller.releaseAt({210, 590}, Qt::LeftButton);
        expect(controller.state().selectedNodes.size() == 2,
               "selection rectangle must select enclosed anchors");

        AnchorEditor::MenuInfo info;
        expect(controller.prepareMenu({300, 400}, info), "last anchor must open its menu");
        expect(!info.interpolationEnabled, "last anchor interpolation must be disabled");
        delete source;
    }

    void testBoundaryClippingAndRejectedMutation() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        int publishes = 0;
        controller.setHostCallbacks({
            [] { return false; },
            [&](const QList<AnchorCurve *> &) { ++publishes; },
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        controller.setEditActive(true);
        controller.doubleClickAt({-100, -500}, Qt::LeftButton);
        expect(controller.curves().isEmpty(), "rejected transaction must not mutate curves");
        expect(publishes == 0, "rejected transaction must not publish");

        controller.setHostCallbacks({
            [] { return true; },
            [&](const QList<AnchorCurve *> &) { ++publishes; },
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        controller.doubleClickAt({-100, -500}, Qt::LeftButton);
        if (controller.curves().isEmpty()) {
            expect(false, "accepted boundary create must produce a curve");
            return;
        }
        const auto *node = controller.curves().first()->nodes().toList().first();
        expect(node->pos() == 0, "created anchor tick must be clipped to zero");
        expect(node->value() == 100, "host mapper must clip created anchor value");
    }

    void testTransferAndMerge() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        auto *left = makeCurve({
            {10, 20},
            {20, 40}
        });
        auto *right = makeCurve({
            {40, 60},
            {50, 80}
        });
        controller.setHostCallbacks({
            [] { return true; },
            [](const QList<AnchorCurve *> &) {},
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        controller.loadFromModel({left, right});
        controller.setEditActive(true);

        controller.pressAt({200, 600}, Qt::LeftButton);
        controller.moveAt({450, 400}, Qt::LeftButton);
        controller.releaseAt({450, 400}, Qt::LeftButton);
        expect(controller.curves().size() == 2, "cross-curve transfer must retain both curves");
        expect(controller.curves().at(0)->nodes().toList().size() == 1,
               "cross-curve transfer must remove the source node");
        expect(controller.curves().at(1)->nodes().toList().size() == 3,
               "cross-curve transfer must insert the target node");

        controller.exitEditing();
        controller.pressAt({100, 800}, Qt::LeftButton);
        controller.hoverMoveAt({400, 400});
        expect(controller.state().showMergePreview, "adjacent endpoint must offer merge preview");
        controller.pressAt({400, 400}, Qt::LeftButton);
        expect(controller.curves().size() == 1, "endpoint merge must combine adjacent curves");
        delete left;
        delete right;
    }

    void testCallbackOrder() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        QStringList events;
        controller.setHostCallbacks({
            [&] {
                events.append("begin");
                return true;
            },
            [&](const QList<AnchorCurve *> &) { events.append("publish"); },
            [&](AnchorEditor::EditFinishReason) { events.append("finish"); },
            [&] { events.append("state"); },
        });
        controller.setEditActive(true);
        events.clear();
        controller.doubleClickAt({100, 800}, Qt::LeftButton);
        expect(events == QStringList({"begin", "publish", "finish", "state"}),
               "mutation callbacks must follow begin-publish-finish-state order");
    }

    void testAnchorSamplesOverrideDrawOnlyInTheirInterval() {
        auto *draw = new DrawCurve;
        draw->setLocalStart(0);
        draw->setValues({1, 2, 3, 4, 5});
        auto *anchor = makeCurve({
            {10, 20},
            {20, 40}
        });
        auto *sampled = anchor->toDrawCurve();
        const auto merged = AppModelUtils::mergeCurves({}, {draw, sampled});
        expect(merged.size() == 1, "overlapping draw and anchor samples must form one curve");
        if (!merged.isEmpty()) {
            expect(merged.first()->values() == QList<int>({1, 2, 20, 30, 40}),
                   "anchor samples must override draw only inside the anchor interval");
        }

        auto *single = makeCurve({
            {30, 60}
        });
        expect(single->toDrawCurve() == nullptr,
               "single-node anchor must not produce a final parameter sample");

        qDeleteAll(merged);
        delete single;
        delete sampled;
        delete anchor;
        delete draw;
    }

    void testAnchorSamplingDoesNotExtrapolateBeforeFirstNode() {
        auto *anchor = makeCurve({
            {2,  0  },
            {12, 100}
        });
        auto *sampled = anchor->toDrawCurve();
        expect(sampled != nullptr, "two-node anchor must produce samples");
        if (sampled) {
            expect(sampled->localStart() == 0,
                   "anchor samples must remain aligned to the draw-curve grid");
            expect(!sampled->values().isEmpty() && sampled->values().first() == 0,
                   "grid alignment must not extrapolate before the first anchor node");
        }
        delete sampled;
        delete anchor;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    testLoadOwnsCopies();
    testCreateAndPublishingReentry();
    testCreateClearsOverlappingPreview();
    testDragCancelRestoresSnapshot();
    testSelectionDeleteAndInterpolation();
    testKeyboardCommands();
    testCompositionPreservesOtherCurveKind();
    testCompositionRejectsSinglePointDrawCurves();
    testSelectionAndLastNodeMenu();
    testBoundaryClippingAndRejectedMutation();
    testTransferAndMerge();
    testCallbackOrder();
    testAnchorSamplesOverrideDrawOnlyInTheirInterval();
    testAnchorSamplingDoesNotExtrapolateBeforeFirstNode();
    if (failures == 0)
        QTextStream(stdout) << "TestAnchorEditController passed" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
