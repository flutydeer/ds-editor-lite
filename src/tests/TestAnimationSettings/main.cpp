#include "UI/Dialogs/Base/DialogTitleBar.h"

#include <lite/GUI/Animation/IAnimatable.h>
#include <lite/GUI/Controls/ProgressIndicator.h>
#include <lite/GUI/Controls/TapTempoButton.h>
#include <lite/GUI/Controls/ToolTip.h>
#include <lite/GUI/Theme/ThemeManager.h>

#include <QAbstractAnimation>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
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

        [[nodiscard]] int effectiveDuration(
            int duration,
            AnimationGlobal::AnimationLevels minimumLevel = AnimationGlobal::Decreased) const {
            return getEffectiveAnimationTime(duration, minimumLevel);
        }

    protected:
        void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override {
            Q_UNUSED(level)
        }

        void afterSetTimeScale(double scale) override {
            Q_UNUSED(scale)
        }
    };

    void testEffectiveDurationPolicy() {
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(AnimationGlobal::Full, 2.0);
        AnimationProbe probe;

        expect(probe.effectiveDuration(100) == 200,
               "full animations should use the configured time scale");
        expect(probe.effectiveDuration(100, AnimationGlobal::Full) == 200,
               "full-only animations should run at the full level");

        theme->setAnimationSettings(AnimationGlobal::Decreased, 0.5);
        expect(probe.effectiveDuration(100) == 50,
               "lightweight animations should remain enabled at the decreased level");
        expect(probe.effectiveDuration(100, AnimationGlobal::Full) == 0,
               "full-only animations should be immediate at the decreased level");

        theme->setAnimationSettings(AnimationGlobal::None, 2.0);
        expect(probe.effectiveDuration(100) == 0,
               "all decorative animations should be immediate at the none level");
    }

    void testDialogTitleBarRuntimeUpdate() {
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(AnimationGlobal::Full, 2.0);

        QWidget window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        DialogTitleBar titleBar(&window);
        auto *animation = titleBar.findChild<QVariantAnimation *>();
        auto *opacityEffect =
            qobject_cast<QGraphicsOpacityEffect *>(titleBar.graphicsEffect());
        expect(animation && opacityEffect, "dialog title bar should expose its opacity animation");
        if (!animation || !opacityEffect)
            return;

        QEvent deactivateEvent(QEvent::WindowDeactivate);
        QApplication::sendEvent(&window, &deactivateEvent);
        expect(animation->duration() == 600,
               "dialog deactivation should scale the 300 ms transition");
        expect(animation->state() == QAbstractAnimation::Running,
               "dialog deactivation should animate at the full level");

        theme->setAnimationSettings(AnimationGlobal::None, 2.0);
        expect(animation->state() == QAbstractAnimation::Stopped,
               "disabling animations should stop an active title transition");
        expect(qFuzzyCompare(opacityEffect->opacity(), 0.5),
               "disabling animations should snap the title to its inactive endpoint");

        theme->setAnimationSettings(AnimationGlobal::Decreased, 0.5);
        QEvent activateEvent(QEvent::WindowActivate);
        QApplication::sendEvent(&window, &activateEvent);
        expect(animation->duration() == 50,
               "lightweight title opacity should remain scaled at the decreased level");
    }

    void testProgressAndTapTempoLevels() {
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(AnimationGlobal::Full, 2.0);

        ProgressIndicator progress;
        progress.setValue(75);
        const auto progressAnimations = progress.findChildren<QPropertyAnimation *>();
        auto *valueAnimation = progressAnimations.value(0);
        expect(valueAnimation && valueAnimation->duration() == 500,
               "determinate progress should scale its full-level interpolation");

        theme->setAnimationSettings(AnimationGlobal::Decreased, 2.0);
        expect(valueAnimation && valueAnimation->state() == QAbstractAnimation::Stopped,
               "decreased animations should stop determinate progress interpolation");
        expect(qFuzzyCompare(progress.property("apparentValue").toDouble(), 75.0),
               "decreased animations should snap progress to its target value");

        theme->setAnimationSettings(AnimationGlobal::Full, 2.0);
        TapTempoButton tapTempo;
        tapTempo.setProgress(0.75);
        auto *tapAnimation = tapTempo.findChild<QPropertyAnimation *>();
        expect(tapAnimation && tapAnimation->duration() == 300,
               "tap-tempo progress should scale at the full level");

        theme->setAnimationSettings(AnimationGlobal::Decreased, 2.0);
        expect(tapAnimation && tapAnimation->state() == QAbstractAnimation::Stopped,
               "tap-tempo interpolation should stop at the decreased level");
        expect(qFuzzyCompare(tapTempo.progress(), 0.75),
               "tap-tempo interpolation should snap to its target");
    }

    void testToolTipImmediateCompletion() {
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(AnimationGlobal::Full, 2.0);

        ToolTip toolTip(QStringLiteral("Animation settings"));
        toolTip.setAttribute(Qt::WA_DontShowOnScreen);
        auto *animation = toolTip.findChild<QPropertyAnimation *>();
        expect(animation && animation->duration() == 300,
               "tooltip opacity should use the configured time scale");

        toolTip.showAt(QPoint());
        theme->setAnimationSettings(AnimationGlobal::None, 2.0);
        expect(animation && animation->state() == QAbstractAnimation::Stopped,
               "disabling animations should stop a running tooltip transition");
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

} // namespace

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);

    testEffectiveDurationPolicy();
    testDialogTitleBarRuntimeUpdate();
    testProgressAndTapTempoLevels();
    testToolTipImmediateCompletion();

    ThemeManager::instance()->setAnimationSettings(AnimationGlobal::Full, 1.0);
    if (g_failures == 0) {
        QTextStream(stdout) << "All animation settings tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
