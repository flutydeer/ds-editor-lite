#ifndef SPLITLINEINDICATOR_H
#define SPLITLINEINDICATOR_H

#include "UI/Utils/IScalableItem.h"

#include <QGraphicsPathItem>
#include <QPointer>

class NoteView;

class SplitLineIndicator : public QGraphicsPathItem, public IScalableItem {
public:
    explicit SplitLineIndicator(QGraphicsItem *parent = nullptr);

    enum UpdateResult { Hidden, Shown };

    void setPixelsPerQuarterNote(int px);
    UpdateResult updateIndicator(NoteView *noteView, int splitPos);

    [[nodiscard]] NoteView *lastNoteView() const;
    [[nodiscard]] int lastTick() const;
    void setLastTick(int tick);
    void clearState();

protected:
    void afterSetScale() override;
    void afterSetVisibleRect() override;

private:
    void rebuildPath();
    [[nodiscard]] double tickToSceneX(double tick) const;

    QPointer<NoteView> m_lastNoteView;
    int m_splitPos = 0;
    int m_lastTick = 0;
    double m_pixelsPerQuarterNote = 64;
};

#endif // SPLITLINEINDICATOR_H
