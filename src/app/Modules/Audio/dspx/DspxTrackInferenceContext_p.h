#ifndef TALCS_DSPXTRACKINFERENCECONTEXT_P_H
#define TALCS_DSPXTRACKINFERENCECONTEXT_P_H

#include <memory>
#include <QMap>

#include "DspxTrackInferenceContext.h"

namespace talcs {
    class DspxTrackInferenceContextPrivate {
        Q_DECLARE_PUBLIC(DspxTrackInferenceContext)
    public:
        DspxTrackInferenceContext *q_ptr;

        std::unique_ptr<AudioSourceClipSeries> clipSeries;

        QMap<int, DspxSingingClipInferenceContext *> clips;
    };
}

#endif //TALCS_DSPXTRACKINFERENCECONTEXT_P_H
