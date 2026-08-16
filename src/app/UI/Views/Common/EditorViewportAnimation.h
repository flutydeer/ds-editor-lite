#ifndef EDITORVIEWPORTANIMATION_H
#define EDITORVIEWPORTANIMATION_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QObject>
#include <QPointF>
#include <QVariantAnimation>

#include <functional>

class EditorViewportAnimation final : public QObject, public IAnimatable {
public:
    using ApplyCallback = std::function<void(const QPointF &)>;

    explicit EditorViewportAnimation(ApplyCallback apply, QObject *parent = nullptr);

    void moveTo(const QPointF &current, const QPointF &target, bool animated);
    void stop();
    [[nodiscard]] QPointF logicalOffset(const QPointF &current) const;
    [[nodiscard]] bool isRunning() const;

protected:
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

private:
    void updateDuration();

    ApplyCallback m_apply;
    QVariantAnimation m_animation;
    QPointF m_target;
};

#endif // EDITORVIEWPORTANIMATION_H
