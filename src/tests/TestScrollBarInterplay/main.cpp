#include <lite/GUI/Controls/OverlayScrollBar.h>

#include "UI/Views/Common/EditorRhiScrollBarController.h"

#include <QGraphicsView>
#include <QScrollBar>
#include <QTextStream>
#include <QApplication>
#include <QWidget>

// Reproduces the piano-roll startup sequence: the custom bar is attached while
// the view is 0x0, then the view is resized, shown, and given a scene.
// Expected final state: handle length/position derived from the source range,
// exactly what the removed scene scrollbar (ScrollBarView) rendered. Before
// the fix, the overlay bar latched the source's default pageStep=10 on the
// first recompute (Qt emits rangeChanged before assigning the new pageStep),
// collapsing the handle to its 20px minimum and leaving it at mid-track.
namespace {
    int g_failures = 0;

    void probe(const char *label, const QScrollBar *source, OverlayScrollBar *bar) {
        QTextStream(stdout)
            << "[" << label << "] src page=" << source->pageStep()
            << " src max=" << source->maximum() << " | bar page=" << bar->pageStep()
            << " bar max=" << bar->maximum() << " bar value=" << bar->value() << Qt::endl;
    }

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
    }
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QGraphicsView view;
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *bar = OverlayScrollBar::install(&view, Qt::Horizontal);
    bar->setFixedHeight(16);
    QScrollBar *source = view.horizontalScrollBar();

    const auto flush = [] {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    };

    // Scene ~2x the viewport, so the startup handle is ~50% and value is 0.
    const int sceneW = 1800, sceneH = 480;
    const int viewW = 900, viewH = 500;
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, sceneW, sceneH);
    view.setScene(&scene);
    view.resize(viewW, viewH);
    view.show();
    flush();
    probe("startup", source, bar);

    expect(bar->maximum() > 0, "a scrolled view must show a range");
    expect(bar->maximum() == source->maximum(), "bar range must mirror the view range");
    expect(bar->pageStep() == source->pageStep(),
           "bar pageStep must copy the source after the recompute");
    expect(bar->value() == source->value(), "bar value must follow the source");
    const double handleFraction =
        double(source->pageStep()) / (source->maximum() + source->pageStep());
    expect(handleFraction > 0.4 && handleFraction < 0.6,
           "source handle length must be ~50% of the track, got " +
               QString::number(handleFraction).toUtf8());

    // Subsequent zoom keeps the copy in sync, and the value follows Qt.
    view.scale(1.4, 1.4);
    flush();
    expect(bar->maximum() == source->maximum(), "bar range must follow a zoom");
    expect(bar->pageStep() == source->pageStep(), "bar pageStep must follow a zoom");
    probe("after zoom", source, bar);

    QWidget rhiViewport;
    rhiViewport.resize(900, 500);
    EditorRhiScrollBarController rhiBars(&rhiViewport, &rhiViewport);
    QPointF requestedOffset(-1, -1);
    QObject::connect(&rhiBars, &EditorRhiScrollBarController::offsetChangeRequested,
                     &rhiViewport,
                     [&requestedOffset](const QPointF &offset) { requestedOffset = offset; });
    rhiBars.setMetrics(QSizeF(1800, 1000), QPointF(0, 0), QSizeF(90, 50));
    rhiViewport.show();
    flush();

    auto *rhiHorizontal = rhiBars.horizontalBar();
    auto *rhiVertical = rhiBars.verticalBar();
    expect(rhiHorizontal->maximum() == 900 && rhiHorizontal->pageStep() == 900,
           "RHI horizontal metrics must describe one visible page");
    expect(rhiVertical->maximum() == 500 && rhiVertical->pageStep() == 500,
           "RHI vertical metrics must describe one visible page");
    expect(rhiHorizontal->isVisible() && rhiVertical->isVisible(),
           "RHI bars with overflow must be visible");
    expect(rhiHorizontal->width() == 884 && rhiVertical->height() == 484,
           "companion RHI bars must leave the bottom-right corner unobstructed");

    rhiHorizontal->setValue(450);
    flush();
    expect(requestedOffset == QPointF(450, 0),
           "dragging an RHI bar must request the corresponding camera offset");

    rhiBars.setMetrics(QSizeF(900, 500), QPointF(0, 0));
    flush();
    expect(!rhiHorizontal->isVisible() && !rhiVertical->isVisible(),
           "RHI bars without overflow must be hidden");

    rhiBars.setMetrics(QSizeF(900.4, 500.4), QPointF(0, 0));
    flush();
    expect(!rhiHorizontal->isVisible() && !rhiVertical->isVisible(),
           "subpixel layout noise must not create a false RHI scroll range");

    if (g_failures == 0) {
        QTextStream(stdout) << "All ScrollBarInterplay tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
