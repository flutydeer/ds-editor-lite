#include "DspxTrackInferenceContext.h"
#include "DspxTrackInferenceContext_p.h"

#include <TalcsCore/AudioSourceClipSeries.h>
#include <TalcsCore/PositionableMixerAudioSource.h>
#include "DspxSingingClipInferenceContext.h"
#include "DspxSingingClipInferenceContext_p.h"

namespace talcs {

    DspxTrackInferenceContext::DspxTrackInferenceContext(QObject *parent) : QObject(parent), d_ptr(new DspxTrackInferenceContextPrivate) {
        Q_D(DspxTrackInferenceContext);
        d->q_ptr = this;
        d->clipSeries = std::make_unique<AudioSourceClipSeries>();
    }

    DspxTrackInferenceContext::~DspxTrackInferenceContext() = default;

    AudioSourceClipSeries *DspxTrackInferenceContext::clipSeries() const {
        Q_D(const DspxTrackInferenceContext);
        return d->clipSeries.get();
    }

    DspxSingingClipInferenceContext *DspxTrackInferenceContext::addSingingClip(int id) {
        Q_D(DspxTrackInferenceContext);
        auto clip = new DspxSingingClipInferenceContext(this);
        auto clipView = d->clipSeries->insertClip(clip->controlMixer(), 0, 0, 1);
        d->clips.insert(id, clip);
        clip->d_func()->clipView = clipView;
        return clip;
    }

    void DspxTrackInferenceContext::removeSingingClip(int id) {
        Q_D(DspxTrackInferenceContext);
        Q_ASSERT(d->clips.contains(id));
        std::unique_ptr<DspxSingingClipInferenceContext> clip(d->clips.take(id));
        d->clipSeries->removeClip(clip->d_func()->clipView);
    }

    QList<DspxSingingClipInferenceContext *> DspxTrackInferenceContext::clips() const {
        Q_D(const DspxTrackInferenceContext);
        return d->clips.values();
    }

}