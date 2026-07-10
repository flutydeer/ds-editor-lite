//
// Created by FlutyDeer on 2026/6/7.
//

#ifndef DS_EDITOR_LITE_INFERENCEAPPLYGATE_H
#define DS_EDITOR_LITE_INFERENCEAPPLYGATE_H

#include "Modules/Inference/Models/InferenceTaskContext.h"

namespace InferenceApplyGate {
    enum class Decision {
        Apply,
        Drop,
        Defer,
    };

    struct Options {
        QString phase = "apply";
        qsizetype expectedNoteCount = -1;
        bool requirePiece = true;
        bool requireNotesInPiece = true;
        bool checkSingerSpeaker = true;
        bool checkEditSession = false;
        bool allowUnchangedPieceRevisionMismatch = false;
    };

    [[nodiscard]] Decision resolve(const InferenceTaskContext &context,
                                   InferenceTaskResolution &resolution,
                                   const Options &options = {});
    void logDecision(const InferenceTaskContext &context, const QString &phase, Decision decision,
                     const QString &reason, quint64 currentRevision = 0);
    void logDrop(const InferenceTaskContext &context, const QString &phase, const QString &reason,
                 quint64 currentRevision = 0);
}

#endif // DS_EDITOR_LITE_INFERENCEAPPLYGATE_H
