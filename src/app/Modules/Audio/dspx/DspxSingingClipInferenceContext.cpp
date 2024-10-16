#include "DspxSingingClipInferenceContext.h"

#include "DspxInferencePieceContext.h"
#include "DspxInferencePieceContext_p.h"
#include "DspxSingingClipInferenceContext_p.h"

#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsCore/FutureAudioSourceClipSeries.h>
#include "DspxTrackInferenceContext_p.h"
#include "Modules/Audio/AudioContext.h"

namespace talcs {
    DspxSingingClipInferenceContext::DspxSingingClipInferenceContext(DspxTrackInferenceContext *trackInferenceContext) : QObject(trackInferenceContext), d_ptr(new DspxSingingClipInferenceContextPrivate) {
        Q_D(DspxSingingClipInferenceContext);
        d->q_ptr = this;
        d->trackInferenceContext = trackInferenceContext;
        d->controlMixer = std::make_unique<PositionableMixerAudioSource>();
        d->pieceClipSeries = std::make_unique<FutureAudioSourceClipSeries>();
        d->pieceClipSeries->setBufferingTarget(AudioContext::instance()->transport()); // TODO
        d->pieceClipSeries->setReadMode(trackInferenceContext->d_func()->computeReadMode());
        d->controlMixer->addSource(d->pieceClipSeries.get());
    }

    DspxSingingClipInferenceContext::~DspxSingingClipInferenceContext() = default;

    DspxTrackInferenceContext *DspxSingingClipInferenceContext::trackInferenceContext() const {
        Q_D(const DspxSingingClipInferenceContext);
        return d->trackInferenceContext;
    }

    PositionableMixerAudioSource *DspxSingingClipInferenceContext::controlMixer() const {
        Q_D(const DspxSingingClipInferenceContext);
        return d->controlMixer.get();
    }

    void DspxSingingClipInferenceContext::setStart(int tick) {
        Q_D(DspxSingingClipInferenceContext);
        if (tick != d->startTick) {
            d->startTick = tick;
            updatePosition();
        }
    }

    int DspxSingingClipInferenceContext::start() const {
        Q_D(const DspxSingingClipInferenceContext);
        return d->startTick;
    }

    void DspxSingingClipInferenceContext::setClipStart(int tick) {
        Q_D(DspxSingingClipInferenceContext);
        if (tick != d->clipStartTick) {
            d->clipStartTick = tick;
            updatePosition();
        }
    }

    int DspxSingingClipInferenceContext::clipStart() const {
        Q_D(const DspxSingingClipInferenceContext);
        return d->clipStartTick;
    }

    void DspxSingingClipInferenceContext::setClipLen(int tick) {
        Q_D(DspxSingingClipInferenceContext);
        if (tick != d->clipLenTick) {
            d->clipLenTick = tick;
            updatePosition();
        }
    }

    int DspxSingingClipInferenceContext::clipLen() const {
        Q_D(const DspxSingingClipInferenceContext);
        return d->clipLenTick;
    }

    void DspxSingingClipInferenceContext::updatePosition() {
        Q_D(DspxSingingClipInferenceContext);
        auto clipSeries = d->trackInferenceContext->d_func()->clipSeries.get();
        auto convertTime = AudioContext::instance()->timeConverter(); // TODO
        auto startSample = convertTime(d->clipStartTick);
        clipSeries->setClipStartPos(d->clipView, startSample);
        auto firstSample = convertTime(d->startTick + d->clipStartTick);
        auto lastSample = convertTime(d->startTick + d->clipStartTick + d->clipLenTick);
        clipSeries->setClipRange(d->clipView, firstSample, qMax(qint64(1), lastSample - firstSample));
    }

    void DspxSingingClipInferenceContext::setData(const QVariant &data) {
        Q_D(DspxSingingClipInferenceContext);
        d->data = data;
    }

    QVariant DspxSingingClipInferenceContext::data() const {
        Q_D(const DspxSingingClipInferenceContext);
        return d->data;
    }

    DspxInferencePieceContext *DspxSingingClipInferenceContext::addInferencePiece(int id) {
        Q_D(DspxSingingClipInferenceContext);
        auto piece = new DspxInferencePieceContext(this);
        auto clipView = d->pieceClipSeries->insertClip(piece->d_func()->futureSource.get(), 0, 0, 1);
        d->inferencePieceContexts.insert(id, piece);
        piece->d_func()->clipView = clipView;
        return piece;
    }

    void DspxSingingClipInferenceContext::removeInferencePiece(int id) {
        Q_D(DspxSingingClipInferenceContext);
        Q_ASSERT(d->inferencePieceContexts.contains(id));
        std::unique_ptr<DspxInferencePieceContext> piece(d->inferencePieceContexts.take(id));
        d->pieceClipSeries->removeClip(piece->d_func()->clipView);
    }

    QList<DspxInferencePieceContext *> DspxSingingClipInferenceContext::inferencePieces() const {
        Q_D(const DspxSingingClipInferenceContext);
        return d->inferencePieceContexts.values();
    }
}