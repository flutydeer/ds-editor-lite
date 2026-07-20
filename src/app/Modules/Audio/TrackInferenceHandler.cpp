#include "TrackInferenceHandler.h"

#include <TalcsDevice/AudioDevice.h>

#include "AudioContext.h"
#include "Model/AppModel/InferPiece.h"
#include "dspx/DspxInferencePieceContext.h"
#include "dspx/DspxSingingClipInferenceContext.h"

#include <filesystem>
#include <TalcsDspx/DspxTrackContext.h>
#include <TalcsCore/Decibels.h>
#include <TalcsCore/AudioSourceClipSeries.h>
#include <TalcsFormat/FormatManager.h>
#include <TalcsFormat/AudioFormatInputSource.h>

#include <Model/AppModel/AppModel.h>
#include <Model/AppModel/Track.h>
#include <Model/AppModel/Clip.h>
#include <Model/AppModel/SingingClip.h>

#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>

#define DEVICE_LOCKER                                                                              \
    talcs::AudioDeviceLocker locker(AudioSystem::outputSystem()->context()->device())

TrackInferenceHandler::TrackInferenceHandler(talcs::DspxTrackContext *trackContext, Track *track)
    : DspxTrackInferenceContext(trackContext), m_track(track), m_trackContext(trackContext) {

    trackContext->trackMixer()->addSource(clipSeries());

    for (const auto clip : track->clips()) {
        if (clip->clipType() != IClip::Singing)
            continue;
        handleSingingClipInserted(static_cast<SingingClip *>(clip));
    }

    connect(appModel, &AppModel::tempoChanged, this, [this] {
        DEVICE_LOCKER;
        handleTimeChanged();
    });

    connect(AudioSystem::outputSystem()->context(),
            &talcs::AbstractOutputContext::sampleRateChanged, this, [this] {
                DEVICE_LOCKER;
                handleTimeChanged();
            });

    connect(static_cast<AudioContext *>(trackContext->projectContext()),
            &AudioContext::exporterCausedTimeChanged, this,
            &TrackInferenceHandler::handleTimeChanged);

    connect(track, &Track::clipChanged, this, [this](const Track::ClipChangeType type, Clip *clip) {
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
    const auto singingClipInferenceContext = addSingingClip(clip->id());
    m_singingClipModelDict.insert(clip, singingClipInferenceContext);

    handleSingingClipPropertyChanged(clip);
    handlePieceChanged(clip, clip->pieces());

    connect(clip, &Clip::propertyChanged, this, [clip, this] {
        DEVICE_LOCKER;
        handleSingingClipPropertyChanged(clip);
    });

    connect(clip, &SingingClip::piecesChanged, this, [clip, this](const auto &pieces) {
        DEVICE_LOCKER;
        handlePieceChanged(clip, pieces);
    });

    connect(clip, &SingingClip::noteChanged, this,
            [clip, this](SingingClip::NoteChangeType type, const QList<Note *> &) {
                switch (type) {
                    case SingingClip::OriginalWordPropertyChange:
                    case SingingClip::EditedPhonemeOffsetChange:
                    case SingingClip::TimeKeyPropertyChange: {
                        DEVICE_LOCKER;
                        syncInferPiecePositions(clip);
                        break;
                    }
                    default:
                        break;
                }
            });
}

void TrackInferenceHandler::handleSingingClipRemoved(SingingClip *clip) {
    handlePieceChanged(clip, {});
    disconnect(clip, nullptr, this, nullptr);
    removeSingingClip(clip->id());
    m_singingClipModelDict.remove(clip);
}

void TrackInferenceHandler::handleSingingClipPropertyChanged(SingingClip *clip) const {
    const auto singingClipInferenceContext = m_singingClipModelDict.value(clip);
    singingClipInferenceContext->setStart(clip->start());
    singingClipInferenceContext->setClipStart(clip->clipStart());
    singingClipInferenceContext->setClipLen(clip->clipLen());

    singingClipInferenceContext->controlMixer()->setGain(
        talcs::Decibels::decibelsToGain(clip->gain()));
    singingClipInferenceContext->controlMixer()->setSilentFlags(clip->mute() ? -1 : 0);

    syncInferPiecePositions(clip);
}

void TrackInferenceHandler::handlePieceChanged(SingingClip *clip,
                                               const QList<InferPiece *> &pieces) {
    QSet<int> pieceIds;
    for (const auto piece : pieces) {
        pieceIds.insert(piece->id());
    }
    QSet<int> repeatedPieceIds;
    for (const auto oldPiece : m_singingClipInferPieces.value(clip)) {
        if (pieceIds.contains(oldPiece->id())) {
            repeatedPieceIds.insert(oldPiece->id());
        } else {
            handlePieceRemoved(clip, oldPiece);
        }
    }
    for (const auto piece : pieces) {
        if (!repeatedPieceIds.contains(piece->id())) {
            handlePieceInserted(clip, piece);
        }
    }
    m_singingClipInferPieces.insert(clip, pieces);
}

void TrackInferenceHandler::handlePieceInserted(SingingClip *clip, InferPiece *inferPiece) {
    const auto singingClipInferenceContext = m_singingClipModelDict.value(clip);
    const auto inferencePieceContext =
        singingClipInferenceContext->addInferencePiece(inferPiece->id());
    m_inferPieceModelDict.insert(inferPiece->id(), inferencePieceContext);
    syncInferPiecePosition(inferPiece);
    handleInferPieceStatusChange(clip->id(), inferPiece->id(), inferPiece->acousticInferStatus);
    connect(inferPiece, &InferPiece::statusChanged, this,
            [clipId = clip->id(), pieceId = inferPiece->id(), this](auto status) {
                handleInferPieceStatusChange(clipId, pieceId, status);
            });
}

void TrackInferenceHandler::handlePieceRemoved(SingingClip *clip, InferPiece *inferPiece) {
    disconnect(inferPiece, nullptr, this, nullptr);
    const auto singingClipInferenceContext = m_singingClipModelDict.value(clip);
    singingClipInferenceContext->removeInferencePiece(inferPiece->id());
    m_inferPieceModelDict.remove(inferPiece->id());
}

void TrackInferenceHandler::handleInferPieceStatusChange(const int clipId, const int pieceId,
                                                         const InferStatus status) const {
    const auto clip = dynamic_cast<SingingClip *>(appModel->findClipById(clipId));
    if (!clip)
        return;
    const auto piece = clip->findPieceById(pieceId);
    if (!piece)
        return;
    const auto inferencePieceContext = m_inferPieceModelDict.value(pieceId);
    if (!inferencePieceContext)
        return;

    {
        DEVICE_LOCKER;
        syncInferPiecePosition(piece);
    }

    switch (status) {
        case Success: {
            auto *io =
                AudioContext::instance()->formatManager()->getFormatLoad(piece->audioPath, {});
            std::unique_ptr<talcs::PositionableAudioSource> contentSrc;
            if (io)
                contentSrc = std::make_unique<talcs::AudioFormatInputSource>(io, true);
            else
                contentSrc = std::make_unique<talcs::AudioSourceClipSeries>();
            auto *bufSrc = AudioContext::instance()->makeBufferable(contentSrc.get(), 2);
            {
                DEVICE_LOCKER;
                inferencePieceContext->determineWithSources(std::move(contentSrc), bufSrc);
            }
            break;
        }
        case Failed: {
            DEVICE_LOCKER;
            if (inferencePieceContext->isDetermined())
                inferencePieceContext->reset();
            inferencePieceContext->determine();
            AudioContext::instance()->handleInferPieceFailed();
            break;
        }
        case Pending:
        case Running:
            if (inferencePieceContext->isDetermined()) {
                DEVICE_LOCKER;
                inferencePieceContext->reset();
            }
    }
}

void TrackInferenceHandler::syncInferPiecePosition(const InferPiece *inferPiece) const {
    if (!inferPiece)
        return;
    const auto inferencePieceContext = m_inferPieceModelDict.value(inferPiece->id());
    if (!inferencePieceContext)
        return;

    const auto start = inferPiece->localStartTick();
    const auto end = inferPiece->localEndTick();
    inferencePieceContext->setPos(start);
    inferencePieceContext->setLength(end - start);
}

void TrackInferenceHandler::syncInferPiecePositions(SingingClip *clip) const {
    for (const auto piece : m_singingClipInferPieces.value(clip)) {
        syncInferPiecePosition(piece);
    }
}

void TrackInferenceHandler::handleTimeChanged() const {
    for (const auto singingClipInferenceContext : clips()) {
        singingClipInferenceContext->updatePosition();
        for (const auto inferencePieceContext : singingClipInferenceContext->inferencePieces()) {
            inferencePieceContext->updatePosition();
        }
    }
}
