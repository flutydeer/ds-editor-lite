#include "UI/Views/EditorCanvas/EditorCanvasTypes.h"
#include "UI/Views/EditorCanvas/EditorViewportController.h"
#include "UI/Views/EditorCanvas/RenderUpdateScheduler.h"
#include "UI/Views/EditorCanvas/ScreenSpaceStrokeTessellator.h"

#include <QCoreApplication>
#include <QTextStream>

#include <limits>

namespace {

    int failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    void testBackendKeys() {
        expect(editorCanvasBackendKey(EditorCanvasBackend::Legacy) == QStringLiteral("legacy"),
               "legacy backend key must be stable");
        expect(editorCanvasBackendFromKey(QStringLiteral("experimental-rhi")) ==
                   EditorCanvasBackend::ExperimentalRhi,
               "RHI backend key must round-trip");
        expect(editorCanvasBackendFromKey(QStringLiteral("unknown")) == EditorCanvasBackend::Legacy,
               "unknown backend keys must fail closed to Legacy");
    }

    void testViewportController() {
        EditorViewportController controller;
        auto signalCount = 0;
        EditorDirtyDomains lastDomains;
        QObject::connect(&controller, &EditorViewportController::stateChanged,
                         [&signalCount, &lastDomains](const EditorViewportState &,
                                                      const EditorDirtyDomains domains) {
                             ++signalCount;
                             lastDomains = domains;
                         });
        controller.setCenter(960.0, 3.5);
        expect(signalCount == 1 && lastDomains.testFlag(EditorDirtyDomain::Camera),
               "center changes must dirty only camera state");
        expect(controller.state().centerTick == 960.0 && controller.state().centerTrack == 3.5,
               "semantic viewport center must be retained");
        expect(!controller.setScale(0.0, 1.0), "non-positive viewport scales must be rejected");
        expect(!controller.setScale(std::numeric_limits<double>::quiet_NaN(), 1.0),
               "non-finite viewport scales must be rejected");
        expect(controller.setScale(2.0, 1.5) && lastDomains.testFlag(EditorDirtyDomain::Geometry),
               "scale changes must invalidate screen-space geometry");
    }

    void testSchedulerCoalescing() {
        RenderUpdateScheduler scheduler;
        auto signalCount = 0;
        EditorDirtyDomains flushed;
        QObject::connect(&scheduler, &RenderUpdateScheduler::updateRequested,
                         [&signalCount, &flushed](const EditorDirtyDomains domains) {
                             ++signalCount;
                             flushed = domains;
                         });
        scheduler.request(EditorDirtyDomain::Geometry);
        scheduler.request(EditorDirtyDomain::Text);
        scheduler.request(EditorDirtyDomain::Selection);
        expect(signalCount == 0, "dirty requests must be deferred to one event turn");
        QCoreApplication::processEvents();
        expect(signalCount == 1 && flushed.testFlag(EditorDirtyDomain::Geometry) &&
                   flushed.testFlag(EditorDirtyDomain::Text) &&
                   flushed.testFlag(EditorDirtyDomain::Selection),
               "dirty domains must be coalesced into one update");
    }

    void testStrokeTessellation() {
        const auto mesh = ScreenSpaceStrokeTessellator::tessellate(
            {
                {0.0,   0.0},
                {100.0, 0.0}
        },
            QColor(255, 255, 255), 1.0F, 2.0, 1.5, 1.25, EditorStrokeJoin::Bevel,
            EditorStrokeCap::Butt);
        expect(mesh.size() >= 18 && mesh.size() % 3 == 0,
               "analytic-AA stroke must contain complete inner, fringe, and cap triangles");
        auto hasTransparentFringe = false;
        auto isPixelSnapped = true;
        for (const auto &vertex : mesh)
            hasTransparentFringe |= vertex.color.alpha() == 0;
        for (const auto &vertex : mesh) {
            const auto physicalY = vertex.position.y() * 1.5 * 1.25;
            isPixelSnapped &=
                qAbs(physicalY - qRound(physicalY)) < 0.001 || vertex.color.alpha() == 0;
        }
        expect(hasTransparentFringe, "analytic-AA stroke mesh must include a transparent fringe");
        expect(isPixelSnapped, "odd-width axis-aligned stroke edges must align to physical pixels");
    }

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    testBackendKeys();
    testViewportController();
    testSchedulerCoalescing();
    testStrokeTessellation();
    if (failures == 0) {
        QTextStream(stdout) << "All editor canvas core tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << failures << " test(s) failed" << Qt::endl;
    return 1;
}
