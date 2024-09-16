//
// Created by fluty on 24-9-15.
//

#ifndef INFERPIECE_H
#define INFERPIECE_H

#include "Interface/IInferPiece.h"

#include <QList>


class Note;

class InferPiece : public QObject, public IInferPiece, public UniqueObject {
    Q_OBJECT
public:
    QString singerName;

    QList<Note *> notes;
    // QList<InferPhoneme> phonemes;
    //
    // InferParam pitch;
    // InferParam breathiness;
    // InferParam velocity;
    // InferParam tension;
    // InferParam gender;
    // InferParam voicing;

    [[nodiscard]] int startTick() const override;
    [[nodiscard]] int endTick() const override;
    [[nodiscard]] InferStatus status() const override;

signals:
    void statusChanged(InferStatus status);

private:
    InferStatus m_status = Pending;
};

inline int InferPiece::startTick() const {
    return notes.first()->rStart();
}

inline int InferPiece::endTick() const {
    return notes.last()->rStart() + notes.last()->length();
}

inline InferStatus InferPiece::status() const {
    return m_status;
}

#endif // INFERPIECE_H
