#include "UI/Dialogs/Base/DialogTitleBar.h"

#include <lite/GUI/Animation/IAnimatable.h>
#include <lite/GUI/Controls/ProgressIndicator.h>
#include <lite/GUI/Controls/TapTempoButton.h>
#include <lite/GUI/Controls/ToolTip.h>
#include <lite/GUI/Theme/ThemeManager.h>

#include <QAbstractAnimation>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QScreen>
#include <QTextStream>
#include <QVariantAnimation>

namespace {

    int g_failures = 0;

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
        return false;
    }

    class AnimationProbe final : public IAnimatable {
    public:
        AnimationProbe() {
            initializeAnimation();
        }

        [[nodiscard]] int effectiveDuration(int duration) const {
            return getEffectiveAnimationTime(duration);
        }

    protected:
        void afterSetAnimationEnabled(bool enabled) override {
            Q_UNUSED(enabled)
        }

        void afterSetTimeScale(double scale) override {
            Q_UNUSED(scale)
        }
    };

    void testEffectiveDurationPolicy() {
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(true, 2.0);
        AnimationProbe probe;

        expect(probe.effectiveDuration(100) == 200,
               "enabled animations should use the configured time scale");

        theme->setAnimationSettings(false, 2.0);
        expect(probe.effectiveDuration(100) == 0, "disabled animations should be immediate");
    }

    void testDialogTitleBarRuntimeUpdate() {
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(true, 2.0);

        QWidget window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        DialogTitleBar titleBar(&window);
        auto *animation = titleBar.findChild<QVariantAnimation *>();
        auto *opacityEffect = qobject_cast<QGraphicsOpacityEffect *>(titleBar.graphicsEffect());
        expect(animation && opacityEffect, "dialog title bar should expose its opacity animation");
        if (!animation || !opacityEffect)
            return;

        QEvent deactivateEvent(QEvent::WindowDeactivate);
        QApplication::sendEvent(&window, &deactivateEvent);
        expect(animation->duration() == 600,
               "dialog deactivation should scale the 300 ms transition");
        expect(animation->state() == QAbstractAnimation::Running,
               "dialog deactivation should animate while enabled");

        theme->setAnimationSettings(false, 2.0);
        expect(animation->state() == QAbstractAnimation::Stopped,
               "disabling animations should stop an active title transition");
        expect(qFuzzyCompare(opacityEffect->opacity(), 0.5),
               "disabling animations should snap the title to its inactive endpoint");

        theme->setAnimationSettings(true, 1.0);
        QEvent activateEvent(QEvent::WindowActivate);
        QApplication::sendEvent(&window, &activateEvent);
        expect(animation->duration() == 100,
               "re-enabling animations should restore the unscaled title transition");
    }

    void testProgressAndTapTempoLevels() {
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(true, 2.0);

        ProgressIndicator progress;
        progress.setValue(75);
        expect(!qFuzzyCompare(progress.property("apparentValue").toDouble(), 75.0),
               "determinate progress should animate instead of snapping when enabled");

        theme->setAnimationSettings(false, 2.0);
        expect(qFuzzyCompare(progress.property("apparentValue").toDouble(), 75.0),
               "progress should snap to its target when animation is disabled");

        theme->setAnimationSettings(true, 2.0);
        TapTempoButton tapTempo;
        tapTempo.setProgress(0.75);
        expect(!qFuzzyCompare(tapTempo.progress(), 0.75),
               "tap-tempo should animate instead of snapping when animation is enabled");

        theme->setAnimationSettings(false, 2.0);
        expect(qFuzzyCompare(tapTempo.progress(), 0.75),
               "tap-tempo should snap to its target when animation is disabled");
    }

    void testToolTipImmediateCompletion() {
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(true, 2.0);

        ToolTip toolTip(QStringLiteral("Animation settings"));
        toolTip.setAttribute(Qt::WA_DontShowOnScreen);

        toolTip.showAt(QPoint());
        expect(!qFuzzyCompare(toolTip.windowOpacity(), 1.0),
               "tooltip presentation should animate when enabled");

        theme->setAnimationSettings(false, 2.0);
        expect(qFuzzyCompare(toolTip.windowOpacity(), 1.0),
               "disabling animations should finish tooltip presentation");

        int finishedCount = 0;
        QObject::connect(&toolTip, &ToolTip::hideAnimationFinished,
                         [&finishedCount] { ++finishedCount; });
        toolTip.hideWithAnimation();
        expect(finishedCount == 1,
               "an immediate tooltip hide should preserve its completion signal");
        expect(!toolTip.isVisible(), "an immediate tooltip hide should hide the widget");
    }

    void testToolTipAnchorScreenClamping() {
        const auto *screen = QApplication::primaryScreen();
        expect(screen, "the tooltip anchor test requires a screen");
        if (!screen)
            return;

        ToolTip toolTip(QStringLiteral("Anchored tooltip"));
        toolTip.setAttribute(Qt::WA_DontShowOnScreen);
        toolTip.setAnimationEnabled(false);
        const auto available = screen->availableGeometry();
        toolTip.showAbove({available.topLeft(), QSize(8, 8)});

        const auto geometry = toolTip.frameGeometry();
        expect(geometry.left() >= available.left() && geometry.top() >= available.top(),
               "an anchored tooltip must remain on the anchor's screen at its top-left edge");
    }

} // namespace

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);

    testEffectiveDurationPolicy();
    testDialogTitleBarRuntimeUpdate();
    testProgressAndTapTempoLevels();
    testToolTipImmediateCompletion();
    testToolTipAnchorScreenClamping();

    ThemeManager::instance()->setAnimationSettings(true, 1.0);
    if (g_failures == 0) {
        QTextStream(stdout) << "All animation settings tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
