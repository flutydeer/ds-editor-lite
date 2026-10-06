#ifndef ANIMATIONUTILS_H
#define ANIMATIONUTILS_H

#include <QSignalBlocker>
#include <QVariantAnimation>

#include <utility>

namespace AnimationUtils {

    // Qt publishes valueChanged for metadata writes - setStartValue(), setEndValue(),
    // setDuration() - and for stop(), even while an animation is stopped. The targets
    // in this editor are stateful: the zoom target re-derives the scroll offset from
    // the scale it is handed, so a stray value does not merely look wrong for one
    // frame, it moves the viewport. A clamped content edge jumps out past the
    // viewport edge and the rest of the animation springs the offset back.
    //
    // Every mutation of a stopped animation goes through here, so only genuine
    // interpolated frames reach the target.
    template <typename Configure>
    void configureSilently(QVariantAnimation &animation, Configure &&configure) {
        const QSignalBlocker blocker(&animation);
        std::forward<Configure>(configure)();
    }

} // namespace AnimationUtils

#endif // ANIMATIONUTILS_H
