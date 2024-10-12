//
// Created by fluty on 24-10-12.
//

#ifndef PIECECONTROLLER_H
#define PIECECONTROLLER_H

#include "ModelChangeHandler.h"
#include "Model/AppModel/InferStatus.h"
#include "Utils/Singleton.h"

#include <TalcsCore/AudioSourceClipSeries.h>
#include <QFuture>

namespace talcs {
    class FutureAudioSourceClipSeries;
    class PositionableAudioSource;
    class FutureAudioSource;
    class AudioSourceClipSeries;
}

class PieceController final : public Singleton<PieceController>, public ModelChangeHandler {
public:
    explicit PieceController() = default;

private:
    class PieceContext {
    public:
        talcs::FutureAudioSource *source = nullptr;
        QFuture<talcs::PositionableAudioSource *> future;
    };

    class SingingClipContext {
    public:
        talcs::FutureAudioSourceClipSeries *clipSeries = nullptr;
        talcs::AudioSourceClipSeries::ClipView clipView;
    };

    void handleTrackInserted(Track *track) override;
    void handleTrackRemoved(Track *track) override;
    void handleSingingClipInserted(SingingClip *clip) override;
    void handleSingingClipRemoved(SingingClip *clip) override;
    void handleClipPropertyChanged(Clip *clip) override;
    void handlePiecesChanged(const QList<InferPiece *> &pieces, SingingClip *clip) override;
    void handlePieceStatusChanged(InferStatus status, InferPiece *piece);
    void insertPiece(InferPiece *piece);
    void removePiece(InferPiece *piece);

    QMap<SingingClip *, QList<InferPiece *>> m_clipPieceDict;
    QMap<Track *, talcs::AudioSourceClipSeries *> m_trackBackendDict;
    QMap<SingingClip *, SingingClipContext> m_clipBackendDict;
    QMap<InferPiece *, PieceContext> m_pieceBackendDict;
};


#endif // PIECECONTROLLER_H
