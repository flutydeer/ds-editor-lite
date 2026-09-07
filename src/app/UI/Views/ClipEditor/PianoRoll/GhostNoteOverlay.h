#ifndef GHOSTNOTEOVERLAY_H
#define GHOSTNOTEOVERLAY_H

#include "GhostNoteSource.h"
#include "UI/Views/Common/TimeOverlayView.h"

// 用单个跟随视口的 overlay 绘制其他轨道的音符（矮条），而不是逐音符建 item ——
// 工程里其他轨道的音符可能上万，逐个建 QGraphicsItem 会拖垮场景与命中测试。
class GhostNoteOverlay final : public TimeOverlayView {
    Q_OBJECT

public:
    GhostNoteOverlay();

    // 只持有指针，源对象的存活期由持有方（PianoRollGraphicsViewPrivate）保证
    void setSource(const GhostNoteSource *source);
    // 当前 clip 的 start()，场景 X 原点
    void setOffset(int offset);

protected:
    void updateRectAndPos() override;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    const GhostNoteSource *m_source = nullptr;
    int m_offset = 0;
};

#endif // GHOSTNOTEOVERLAY_H
