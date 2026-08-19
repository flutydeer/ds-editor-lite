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
        expect(begins == 1 && commits == 0,
               "the first anchor must stay provisional until it forms a curve");
        controller.doubleClickAt({200, 600}, Qt::LeftButton);
        expect(begins == 1 && commits == 1,
               "the second anchor must commit the provisional transaction");
        expect(controller.curves().size() == 1, "reentrant load must be ignored while publishing");
        if (controller.curves().isEmpty())
            return;
        expect(controller.curves().first()->nodes().toList().first()->pos() == 10,
               "created anchor tick must use mapper");
        expect(controller.curves().first()->nodes().toList().size() == 2,
               "the committed anchor curve must contain both nodes");
    }

    void testProvisionalAnchorExitDiscardsWithoutPublishing() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        int begins = 0;
        int publishes = 0;
        int discards = 0;
        controller.setHostCallbacks({
            [&] {
                ++begins;
                return true;
            },
            [&](const QList<AnchorCurve *> &) { ++publishes; },
            [&](const AnchorEditor::EditFinishReason reason) {
                if (reason == AnchorEditor::EditFinishReason::Discard)
                    ++discards;
            },
            [] {},
        });
        controller.setEditActive(true);

        controller.doubleClickAt({100, 800}, Qt::LeftButton);
        expect(controller.curves().size() == 1, "the provisional anchor must remain visible");
        controller.exitEditing();
        expect(controller.curves().isEmpty(), "Escape-style exit must remove the provisional anchor");
        expect(begins == 1 && publishes == 0 && discards == 1,
               "discarding a provisional anchor must not publish history");

        controller.doubleClickAt({100, 800}, Qt::LeftButton);
        AnchorEditor::MenuInfo info;
        expect(!controller.prepareMenu({500, 500}, info),
               "a background context action must not open an anchor menu");
        expect(controller.curves().isEmpty(),
               "background right-click-style exit must remove the provisional anchor");
        expect(begins == 2 && publishes == 0 && discards == 2,
               "both provisional exit paths must discard without publishing");
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

    void testSwitchingAwayFromProvisionalAllowsCommit() {
        auto *source = makeCurve({
            {30, 20},
            {40, 40}
        });

        AnchorEditor::AnchorEditController selectionController;
        selectionController.setCoordinateMapper(mapper());
        int selectionPublishes = 0;
        selectionController.setHostCallbacks({
            [] { return true; },
            [&](const QList<AnchorCurve *> &) { ++selectionPublishes; },
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        selectionController.loadFromModel({source});
        selectionController.setEditActive(true);
        selectionController.doubleClickAt({100, 800}, Qt::LeftButton);
        selectionController.pressAt({300, 800}, Qt::LeftButton);
        selectionController.setSelectedInterpolation(AnchorNode::Linear);
        expect(selectionController.curves().size() == 1 && selectionPublishes == 1,
               "switching to an existing curve must discard the provisional curve and commit");
        if (!selectionController.curves().isEmpty()) {
            expect(selectionController.curves().first()->nodes().toList().first()->interpMode() ==
                       AnchorNode::Linear,
                   "the selected existing curve must remain editable after a provisional curve");
        }

        AnchorEditor::AnchorEditController insertionController;
        insertionController.setCoordinateMapper(mapper());
        int insertionPublishes = 0;
        insertionController.setHostCallbacks({
            [] { return true; },
            [&](const QList<AnchorCurve *> &) { ++insertionPublishes; },
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        insertionController.loadFromModel({source});
        insertionController.setEditActive(true);
        insertionController.doubleClickAt({100, 800}, Qt::LeftButton);
        insertionController.doubleClickAt({350, 700}, Qt::LeftButton);
        expect(insertionController.curves().size() == 1 &&
                   insertionController.curves().first()->nodes().toList().size() == 3 &&
                   insertionPublishes == 1,
               "adding to an existing curve must replace and commit the provisional curve");
        delete source;
    }

    void testDeleteToOneRemovesWholeCurve() {
        AnchorEditor::AnchorEditController controller;
        controller.setCoordinateMapper(mapper());
        auto *source = makeCurve({
            {10, 20},
            {20, 40}
        });
        int publishes = 0;
        int publishedCurveCount = -1;
        int commits = 0;
        controller.setHostCallbacks({
            [] { return true; },
            [&](const QList<AnchorCurve *> &curves) {
                ++publishes;
                publishedCurveCount = curves.size();
            },
            [&](const AnchorEditor::EditFinishReason reason) {
                if (reason == AnchorEditor::EditFinishReason::Commit)
                    ++commits;
            },
            [] {},
        });
        controller.loadFromModel({source});
        controller.setEditActive(true);
        controller.pressAt({200, 600}, Qt::LeftButton);
        controller.deleteSelectedNodes();

        expect(controller.curves().isEmpty(),
               "deleting a two-node curve down to one must remove the remaining node");
        expect(publishes == 1 && publishedCurveCount == 0 && commits == 1,
               "removing the whole curve must be committed as one history entry");
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

    void testCompositionRejectsIncompleteAnchorCurves() {
        auto *single = makeCurve({
            {10, 20}
        });
        auto *complete = makeCurve({
            {20, 40},
            {30, 60}
        });

        auto anchorResult = AnchorEditor::replaceAnchors({}, {single, complete});
        expect(anchorResult.size() == 1 && anchorResult.first()->type() == Curve::Anchor,
               "anchor replacement must reject incomplete curves");

        QList<Curve *> existing{single, complete};
        auto drawResult = AnchorEditor::replaceDrawCurves(existing, {});
        expect(drawResult.size() == 1 && drawResult.first()->type() == Curve::Anchor,
               "draw replacement must not preserve incomplete anchor curves");

        qDeleteAll(anchorResult);
        qDeleteAll(drawResult);
        delete complete;
        delete single;
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
        expect(controller.curves().size() == 1,
               "cross-curve transfer must remove an incomplete source curve");
        expect(controller.curves().first()->nodes().toList().size() == 3,
               "cross-curve transfer must insert the target node");

        AnchorEditor::AnchorEditController mergeController;
        mergeController.setCoordinateMapper(mapper());
        mergeController.setHostCallbacks({
            [] { return true; },
            [](const QList<AnchorCurve *> &) {},
            [](AnchorEditor::EditFinishReason) {},
            [] {},
        });
        mergeController.loadFromModel({left, right});
        mergeController.setEditActive(true);
        mergeController.pressAt({100, 800}, Qt::LeftButton);
        mergeController.hoverMoveAt({400, 400});
        expect(mergeController.state().showMergePreview,
               "adjacent endpoint must offer merge preview");
        mergeController.pressAt({400, 400}, Qt::LeftButton);
        expect(mergeController.curves().size() == 1,
               "endpoint merge must combine adjacent curves");
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
        controller.doubleClickAt({200, 600}, Qt::LeftButton);
        expect(events == QStringList({"begin", "state", "publish", "finish", "state"}),
               "provisional creation must publish only after the second anchor");
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
    testProvisionalAnchorExitDiscardsWithoutPublishing();
    testCreateClearsOverlappingPreview();
    testSwitchingAwayFromProvisionalAllowsCommit();
    testDragCancelRestoresSnapshot();
    testSelectionDeleteAndInterpolation();
    testDeleteToOneRemovesWholeCurve();
    testCompositionPreservesOtherCurveKind();
    testCompositionRejectsSinglePointDrawCurves();
    testCompositionRejectsIncompleteAnchorCurves();
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
