#include "TrackInferenceHandler.h"

#include <TalcsDevice/AudioDevice.h>

#include "AudioContext.h"
#include "Model/AppModel/InferPiece.h"
#include "dspx/DspxInferencePieceContext.h"
#include "dspx/DspxSingingClipInferenceContext.h"

#include <filesystem>
#include <TalcsDspx/DspxTrackContext.h>
#include <TalcsCore/Decibels.h>

#include <Model/AppModel/AppModel.h>
#include <Model/AppModel/Track.h>
#include <Model/AppModel/Clip.h>
#include <Model/AppModel/SingingClip.h>

#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>

#define DEVICE_LOCKER                                                                              \
talcs::AudioDeviceLocker locker(AudioSystem::outputSystem()->context()->device())

TrackInferenceHandler::TrackInferenceHandler(talcs::DspxTrackContext *trackContext, Track *track) : DspxTrackInferenceContext(trackContext), m_track(track), m_trackContext(trackContext) {

    trackContext->trackMixer()->addSource(clipSeries());

    for (auto clip : track->clips()) {
        if (clip->clipType() != IClip::Singing)
            continue;
        handleSingingClipInserted(static_cast<SingingClip *>(clip));
    }

    connect(appModel, &AppModel::tempoChanged, this, [=] {
        DEVICE_LOCKER;
        handleTimeChanged();
    });

    connect(AudioSystem::outputSystem()->context(),
            &talcs::AbstractOutputContext::sampleRateChanged, this, [=] {
                DEVICE_LOCKER;
                handleTimeChanged();
            });

    connect(static_cast<AudioContext *>(trackContext->projectContext()), &AudioContext::exporterCausedTimeChanged, this, &TrackInferenceHandler::handleTimeChanged);

    connect(track, &Track::clipChanged, this, [=](Track::ClipChangeType type, Clip *clip) {
        if (clip->clipType() != IClip::Singing)
            return;
        DEVICE_LOCKER;
        switch (type) {
            case Track::Inserted:
                handleSingingClipInserted(static_cast<SingingClip *>(clip));
                break;
            case Track::Removed:
                handleSingingClipRemoved(static_cast<SingingClip *>(clip));
                break;
        }
    });

}

TrackInferenceHandler::~TrackInferenceHandler() {
}

void TrackInferenceHandler::handleSingingClipInserted(SingingClip *clip) {
    auto singingClipInferenceContext = addSingingClip(clip->id());
    m_singingClipModelDict.insert(clip, singingClipInferenceContext);

    handleSingingClipPropertyChanged(clip);
    handlePieceChanged(clip, clip->pieces());

    connect(clip, &Clip::propertyChanged, this, [=] {
        DEVICE_LOCKER;
        handleSingingClipPropertyChanged(clip);
    });

    connect(clip, &SingingClip::piecesChanged, this, [=](const auto &pieces) {
        DEVICE_LOCKER;
        handlePieceChanged(clip, pieces);
    });
}

void TrackInferenceHandler::handleSingingClipRemoved(SingingClip *clip) {
    handlePieceChanged(clip, {});
    removeSingingClip(clip->id());
    m_singingClipModelDict.remove(clip);
}

void TrackInferenceHandler::handleSingingClipPropertyChanged(SingingClip *clip) {
    auto singingClipInferenceContext = m_singingClipModelDict.value(clip);
    singingClipInferenceContext->setStart(clip->start());
    singingClipInferenceContext->setClipStart(clip->clipStart());
    singingClipInferenceContext->setClipLen(clip->clipLen());

    singingClipInferenceContext->controlMixer()->setGain(talcs::Decibels::decibelsToGain(clip->gain()));
    singingClipInferenceContext->controlMixer()->setSilentFlags(clip->mute() ? -1 : 0);
}

void TrackInferenceHandler::handlePieceChanged(SingingClip *clip, const QList<InferPiece *> &pieces) {
    for (auto oldPiece : m_singingClipInferPieces.value(clip)) {
        handlePieceRemoved(clip, oldPiece);
    }
    for (auto piece : pieces) {
        handlePieceInserted(clip, piece);
    }
    m_singingClipInferPieces.insert(clip, pieces);
}

void TrackInferenceHandler::handlePieceInserted(SingingClip *clip, InferPiece *inferPiece) {
    auto singingClipInferenceContext = m_singingClipModelDict.value(clip);
    auto inferencePieceContext = singingClipInferenceContext->addInferencePiece(inferPiece->id());
    m_inferPieceModelDict.insert(inferPiece, inferencePieceContext);
    inferencePieceContext->setPos(inferPiece->realStartTick());
    inferencePieceContext->setLength(inferPiece->realEndTick() - inferPiece->realStartTick());
    handleInferPieceStatusChange(inferPiece, inferPiece->acousticInferStatus);
    connect(inferPiece, &InferPiece::statusChanged, this, [=](auto status) {
        DEVICE_LOCKER;
        handleInferPieceStatusChange(inferPiece, status);
    });
}

void TrackInferenceHandler::handlePieceRemoved(SingingClip *clip, InferPiece *inferPiece) {
    auto singingClipInferenceContext = m_singingClipModelDict.value(clip);
    singingClipInferenceContext->removeInferencePiece(inferPiece->id());
    m_inferPieceModelDict.remove(inferPiece);
}

void TrackInferenceHandler::handleInferPieceStatusChange(InferPiece *piece, InferStatus status) {
    auto inferencePieceContext = m_inferPieceModelDict.value(piece);
    switch (status) {
        case Success:
            inferencePieceContext->determine(piece->audioPath);
            break;
        case Failed:
            inferencePieceContext->determine();
            break;
        case Pending:
        case Running:
            if (inferencePieceContext->isDetermined())
                inferencePieceContext->reset();
    }
}

void TrackInferenceHandler::handleTimeChanged() {
    for (auto singingClipInferenceContext : clips()) {
        singingClipInferenceContext->updatePosition();
        for (auto inferencePieceContext : singingClipInferenceContext->inferencePieces()) {
            inferencePieceContext->updatePosition();
        }
    }
}
