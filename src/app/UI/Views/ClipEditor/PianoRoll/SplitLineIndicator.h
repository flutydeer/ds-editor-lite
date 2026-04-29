#ifndef SPLITLINEINDICATOR_H
#define SPLITLINEINDICATOR_H

#include <QGraphicsPathItem>
#include <QPointer>

class NoteView;

class SplitLineIndicator : public QGraphicsPathItem {
public:
    explicit SplitLineIndicator(QGraphicsItem *parent = nullptr);

    enum UpdateResult { Hidden, Shown };

    UpdateResult updateIndicator(NoteView *noteView, int splitPos, double sceneX);

    [[nodiscard]] NoteView *lastNoteView() const;
    [[nodiscard]] int lastTick() const;
    void setLastTick(int tick);
    void clearState();

private:
    void buildPath(double x, NoteView *noteView);

    QPointer<NoteView> m_lastNoteView;
    int m_lastTick = 0;
};

#endif // SPLITLINEINDICATOR_H
