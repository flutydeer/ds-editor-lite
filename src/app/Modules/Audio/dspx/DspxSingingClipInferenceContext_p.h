#ifndef TALCS_DSPXSINGINGCLIPINFERENCECONTEXT_P_H
#define TALCS_DSPXSINGINGCLIPINFERENCECONTEXT_P_H

#include "DspxSingingClipInferenceContext.h"

#include <memory>

#include <QMap>
#include <QVariant>

#include <TalcsCore/AudioSourceClipSeries.h>

namespace talcs {

    class FutureAudioSourceClipSeries;

    class DspxSingingClipInferenceContextPrivate {
        Q_DECLARE_PUBLIC(DspxSingingClipInferenceContext)
    public:
        DspxSingingClipInferenceContext *q_ptr;

        DspxTrackInferenceContext *trackInferenceContext;

        AudioSourceClipSeries::ClipView clipView;
        std::unique_ptr<FutureAudioSourceClipSeries> pieceClipSeries;
        std::unique_ptr<PositionableMixerAudioSource> controlMixer;

        int startTick = 0;
        int clipStartTick = 0;
        int clipLenTick = 0;

        QMap<int, DspxInferencePieceContext *> inferencePieceContexts;

        QVariant data;
    };
}

#endif // TALCS_DSPXSINGINGCLIPINFERENCECONTEXT_P_H
