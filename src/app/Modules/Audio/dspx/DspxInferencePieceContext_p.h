#ifndef TALCS_DSPXINFERENCEPIECECONTEXT_P_H
#define TALCS_DSPXINFERENCEPIECECONTEXT_P_H

#include "DspxInferencePieceContext.h"

#include <memory>
#include <QPromise>

#include <TalcsCore/FutureAudioSourceClipSeries.h>

namespace talcs {
    class BufferingAudioSource;
}

namespace talcs {
    class FutureAudioSource;
}

namespace talcs {

    class PositionableAudioSource;

    class DspxInferencePieceContextPrivate {
        Q_DECLARE_PUBLIC(DspxInferencePieceContext)
    public:
        DspxInferencePieceContext *q_ptr;

        DspxSingingClipInferenceContext *singingClipInferenceContext;

        FutureAudioSourceClipSeries::ClipView clipView;

        std::unique_ptr<PositionableAudioSource> contentSrc;
        std::unique_ptr<BufferingAudioSource> bufSrc;

        QPromise<PositionableAudioSource *> promise;
        std::unique_ptr<FutureAudioSource> futureSource;

        qint64 preloadingBufferSize = 0;
        double preloadingSampleRate = 0.0;

        int posTick = 0;
        int lengthTick = 0;
    };
}

#endif // TALCS_DSPXINFERENCEPIECECONTEXT_P_H
