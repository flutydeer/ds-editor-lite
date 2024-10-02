//
// Created by fluty on 24-9-15.
//

#ifndef INFERPIECE_H
#define INFERPIECE_H

#include "Interface/IInferPiece.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/DrawCurve.h"
#include "Utils/Property.h"

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

    [[nodiscard]] int noteStartTick() const override;
    [[nodiscard]] int noteEndTick() const override;
    int realStartTick() const;
    int realEndTick() const;

signals:
    void statusChanged(InferStatus status);
};

inline int InferPiece::clipId() const {
    return clip->id();
}

inline int InferPiece::noteStartTick() const {
    return notes.first()->rStart();
}

inline int InferPiece::noteEndTick() const {
    return notes.last()->rStart() + notes.last()->length();
}

// TODO: 范围需要重构
inline int InferPiece::realStartTick() const {
    auto firstNote = notes.first();
    auto phoneInfo = firstNote->phonemeOffsetInfo();
    auto aheadInfo = phoneInfo.ahead.result();
    auto normalInfo = phoneInfo.normal.result();
    auto phoneOffsets = aheadInfo.isEmpty()? normalInfo : aheadInfo;
    auto firstOffset = phoneOffsets.first();
    auto paddingTicks = appModel->msToTick(100 + firstOffset); // SP 0.1s
    return noteStartTick() - paddingTicks;
    // return noteStartTick();
}

inline int InferPiece::realEndTick() const {
    auto paddingTicks = appModel->msToTick(100); // SP 0.1s
    return noteEndTick() + paddingTicks;
    // return noteEndTick();
}

#endif // INFERPIECE_H
