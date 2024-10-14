#include "DspxInferencePieceContext.h"
#include "DspxInferencePieceContext_p.h"

#include "DspxSingingClipInferenceContext_p.h"

#include <TalcsFormat/FormatManager.h>
#include <TalcsFormat/AudioFormatInputSource.h>
#include "Modules/Audio/AudioContext.h"

#include <TalcsCore/FutureAudioSource.h>
#include <TalcsCore/BufferingAudioSource.h>

namespace talcs {

    DspxInferencePieceContext::DspxInferencePieceContext(DspxSingingClipInferenceContext *singingClipInferenceContext) : QObject(singingClipInferenceContext), d_ptr(new DspxInferencePieceContextPrivate) {
        Q_D(DspxInferencePieceContext);
        d->q_ptr = this;
        d->singingClipInferenceContext = singingClipInferenceContext;
        d->promise.setProgressRange(0, 48000);
        d->promise.start();
        d->futureSource = std::make_unique<FutureAudioSource>(d->promise.future());
    }

    DspxInferencePieceContext::~DspxInferencePieceContext() = default;

    DspxSingingClipInferenceContext *DspxInferencePieceContext::singingClipInferenceContext() const {
        Q_D(const DspxInferencePieceContext);
        return d->singingClipInferenceContext;
    }

    void DspxInferencePieceContext::setPos(int tick) {
        Q_D(DspxInferencePieceContext);
        if (tick != d->posTick) {
            d->posTick = tick;
            updatePosition();
        }
    }

    int DspxInferencePieceContext::pos() const {
        Q_D(const DspxInferencePieceContext);
        return d->posTick;
    }

    void DspxInferencePieceContext::setLength(int tick) {
        Q_D(DspxInferencePieceContext);
        if (tick != d->lengthTick) {
            d->lengthTick = tick;
            updatePosition();
        }
    }

    int DspxInferencePieceContext::length() const {
        Q_D(const DspxInferencePieceContext);
        return d->lengthTick;
    }

    void DspxInferencePieceContext::updatePosition() {
        Q_D(DspxInferencePieceContext);
        auto clipSeries = d->singingClipInferenceContext->d_func()->pieceClipSeries.get();
        auto convertTime = AudioContext::instance()->timeConverter();
        clipSeries->setClipStartPos(d->clipView, 0);
        auto clipPositionSample = convertTime(d->singingClipInferenceContext->start());
        auto firstSample = convertTime(d->singingClipInferenceContext->start() + d->posTick) - clipPositionSample;
        auto lastSample = convertTime(d->singingClipInferenceContext->start() + d->posTick + d->lengthTick) - clipPositionSample;
        clipSeries->setClipRange(d->clipView, firstSample, qMax(qint64(1), lastSample - firstSample));
    }

    bool DspxInferencePieceContext::determine(const QString &audioFilePath) {
        Q_D(DspxInferencePieceContext);
        auto io = AudioContext::instance()->formatManager()->getFormatLoad(audioFilePath, {});
        if (!io) {
            d->contentSrc = std::make_unique<AudioSourceClipSeries>();
        } else {
            d->contentSrc = std::make_unique<AudioFormatInputSource>(io, true);
        }
        d->bufSrc.reset(AudioContext::instance()->makeBufferable(d->contentSrc.get(), 2));
        d->promise.addResult(d->bufSrc.get());
        d->promise.finish();
        return static_cast<bool>(io);
    }

    void DspxInferencePieceContext::reset() {
        Q_D(DspxInferencePieceContext);
        QPromise<PositionableAudioSource *> promise;
        promise.start();
        std::unique_ptr<FutureAudioSource> futureSource = std::make_unique<FutureAudioSource>(promise.future());

        d->singingClipInferenceContext->d_func()->pieceClipSeries->setClipContent(d->clipView, futureSource.get());

        d->promise = std::move(promise);
        d->futureSource = std::move(futureSource);

        d->bufSrc.reset();
        d->contentSrc.reset();

    }

    bool DspxInferencePieceContext::isDetermined() const {
        Q_D(const DspxInferencePieceContext);
        return static_cast<bool>(d->bufSrc);
    }
}