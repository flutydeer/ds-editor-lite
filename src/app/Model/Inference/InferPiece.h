//
// Created by fluty on 24-9-15.
//

#ifndef INFERPIECE_H
#define INFERPIECE_H

#include "Interface/IInferPiece.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/DrawCurve.h"

#include <QList>

class Note;

class InferPiece : public QObject, public IInferPiece {
    Q_OBJECT
public:
    explicit InferPiece(SingingClip *clip) : QObject(clip), clip(clip) {
        acousticInferStatus.setNotify(qSignalCallback(statusChanged));
    };

    [[nodiscard]] int clipId() const override;

    SingingClip *clip;
    QString singerName;
    QList<Note *> notes;
    DrawCurve pitch;
    Property<InferStatus> acousticInferStatus = Pending;
    bool dirty = false;

    [[nodiscard]] int startTick() const override;
    [[nodiscard]] int endTick() const override;

signals:
    void statusChanged(InferStatus status);
};

inline int InferPiece::clipId() const {
    return clip->id();
}

inline int InferPiece::startTick() const {
    return notes.first()->rStart();
}

inline int InferPiece::endTick() const {
    return notes.last()->rStart() + notes.last()->length();
}

#endif // INFERPIECE_H
