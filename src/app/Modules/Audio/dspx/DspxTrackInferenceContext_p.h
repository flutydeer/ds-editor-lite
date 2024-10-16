#ifndef TALCS_DSPXTRACKINFERENCECONTEXT_P_H
#define TALCS_DSPXTRACKINFERENCECONTEXT_P_H

#include <memory>
#include <QMap>

#include "DspxTrackInferenceContext.h"

#include <TalcsCore/FutureAudioSourceClipSeries.h>

namespace talcs {
    class DspxTrackInferenceContextPrivate {
        Q_DECLARE_PUBLIC(DspxTrackInferenceContext)
    public:
        DspxTrackInferenceContext *q_ptr;

        std::unique_ptr<AudioSourceClipSeries> clipSeries;

        QMap<int, DspxSingingClipInferenceContext *> clips;

        DspxTrackInferenceContext::Mode mode = DspxTrackInferenceContext::Default;

        constexpr FutureAudioSourceClipSeries::ReadMode computeReadMode() const {
            if (mode == DspxTrackInferenceContext::Default)
                return FutureAudioSourceClipSeries::Notify;
            if (mode == DspxTrackInferenceContext::Export)
                return FutureAudioSourceClipSeries::Block;
            return FutureAudioSourceClipSeries::Skip;
        }
    };
}

#endif //TALCS_DSPXTRACKINFERENCECONTEXT_P_H
