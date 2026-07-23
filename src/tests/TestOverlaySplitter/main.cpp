#include "UI/Controls/OverlaySplitter.h"

#include <QApplication>
#include <QTextStream>

namespace {

    int g_failures = 0;

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
        return false;
    }

    SplitterOverlayGrip *findGrip(QWidget *parent) {
        const auto grips =
            parent->findChildren<SplitterOverlayGrip *>(QString(), Qt::FindDirectChildrenOnly);
        expect(grips.size() <= 1, "a host must not contain duplicate overlay grips");
        return grips.value(0);
    }

    void processVisibilityEvents() {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }

} // namespace

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);

    QWidget firstHost;
    firstHost.setAttribute(Qt::WA_DontShowOnScreen);
    firstHost.resize(640, 480);
    firstHost.show();

    auto *splitter = new OverlaySplitter(Qt::Vertical, &firstHost);
    splitter->setGeometry(firstHost.rect());
    splitter->addWidget(new QWidget);
    splitter->addWidget(new QWidget);
    splitter->show();
    processVisibilityEvents();

    auto *grip = findGrip(&firstHost);
    expect(grip, "showing a two-pane splitter must create its overlay grip");
    expect(grip && grip->isVisible(), "the grip must be visible with the splitter");

    splitter->hide();
    processVisibilityEvents();
    expect(grip && !grip->isVisible(), "hiding the splitter must hide its sibling grip");

    splitter->show();
    processVisibilityEvents();
    expect(grip && grip->isVisible(), "showing the splitter again must restore its grip");

    QWidget secondHost;
    secondHost.setAttribute(Qt::WA_DontShowOnScreen);
    secondHost.resize(640, 480);
    secondHost.show();
    splitter->setParent(&secondHost);
    splitter->setGeometry(secondHost.rect());
    splitter->show();
    processVisibilityEvents();

    expect(findGrip(&firstHost) == nullptr, "reparenting must remove the grip from the old host");
    expect(findGrip(&secondHost) == grip,
           "reparenting must move the existing grip to the new host");
    expect(grip && grip->isVisible(), "the reparented grip must follow splitter visibility");

    delete splitter;
    processVisibilityEvents();
    expect(findGrip(&secondHost) == nullptr,
           "destroying the splitter must destroy its sibling grip");

    if (g_failures == 0) {
        QTextStream(stdout) << "All OverlaySplitter tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
