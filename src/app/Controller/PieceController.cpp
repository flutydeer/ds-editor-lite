//
// Created by fluty on 24-10-12.
//

#include "PieceController.h"

#include "Model/AppModel/InferPiece.h"
#include "Modules/Audio/AudioContext.h"

#include <TalcsCore/AudioSourceClipSeries.h>
#include <TalcsDspx/DspxTrackContext.h>
#include <TalcsCore/FutureAudioSource.h>
#include <TalcsCore/FutureAudioSourceClipSeries.h>

void PieceController::handleTrackInserted(Track *track) {
    auto series = new talcs::AudioSourceClipSeries;
    m_trackBackendDict[track] = series;
    auto trackContext = AudioContext::instance()->getContextFromTrack(track);
    trackContext->trackMixer()->addSource(series);
    ModelChangeHandler::handleTrackInserted(track);
}

void PieceController::handleTrackRemoved(Track *track) {
    ModelChangeHandler::handleTrackRemoved(track);
    m_trackBackendDict.remove(track);
}

void PieceController::handleSingingClipInserted(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipInserted(clip);
    auto futureSeries = new talcs::FutureAudioSourceClipSeries;
    Track *track;
    appModel->findClipById(clip->id(), track);
    auto trackSeries = m_trackBackendDict[track];
    auto clipView = trackSeries->insertClip(futureSeries, 0, 0, 1);
    m_clipBackendDict[clip] = {futureSeries, clipView};
    handleClipPropertyChanged(clip);
}

void PieceController::handleSingingClipRemoved(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipRemoved(clip);
    m_clipBackendDict.remove(clip);
}

void PieceController::handleClipPropertyChanged(Clip *clip) {
    ModelChangeHandler::handleClipPropertyChanged(clip);
    if (clip->clipType() != IClip::Singing)
        return;
    // auto singingClip = reinterpret_cast<SingingClip *>(clip);
    // auto context = m_clipBackendDict[singingClip];
    // Track *track;
    // appModel->findClipById(clip->id(), track);
    // auto trackContext = AudioContext::instance()->getContextFromTrack(track);
    // auto convertTime = trackContext->projectContext()->timeConverter();
    // auto startSample = convertTime(clip->start());
    // context.clipSeries->setClipStartPos(context.clipView, startSample);
    // auto firstSample = convertTime(clip->start() + clip->clipStart());
    // auto lastSample = convertTime(clip->start() + clip->clipStart() + clip->clipLen());
    // context.clipSeries->setClipRange(context.clipView, firstSample,
    //                                  qMax(qint64(1), lastSample - firstSample));
}

void PieceController::handlePiecesChanged(const QList<InferPiece *> &pieces, SingingClip *clip) {
    ModelChangeHandler::handlePiecesChanged(pieces, clip);
    auto &oldPieces = m_clipPieceDict[clip];
    QList<InferPiece *> newPieces;
    for (const auto &piece : pieces) {
        bool exists = false;
        for (int i = 0; i < oldPieces.count(); i++) {
            if (oldPieces[i] == piece) {
                exists = true;
                newPieces.append(oldPieces[i]);
                oldPieces.removeAt(i);
                break;
            }
        }
        if (!exists) {
            insertPiece(piece);
        }
    }
    QList<InferPiece *> removedPieces = oldPieces;
    m_clipPieceDict[clip] = newPieces;
    for (auto &piece : removedPieces) {
        disconnect(piece, nullptr, this, nullptr);
        removePiece(piece);
    }
}

void PieceController::handlePieceStatusChanged(InferStatus status, InferPiece *piece) {
    auto context = m_pieceBackendDict[piece];
    switch (status) {
        case Pending:
        case Running:
            break;
        case Success:
            qDebug() << "Piece ready" << piece->audioPath;
            break;
        case Failed:
            removePiece(piece);
            break;
        default:
            break;
    }
}

void PieceController::insertPiece(InferPiece *piece) {
    // auto clipContext = m_clipBackendDict[piece->clip];
    // talcs::FutureAudioSource::Callbacks callbacks;
    // QFuture<talcs::PositionableAudioSource *> future;
    // auto source = new talcs::FutureAudioSource({future}, callbacks);
    // clipContext.clipSeries->insertClip(source, 0, 0, 1);
    // m_pieceBackendDict[piece] = {source, future};
    //
    // Track *track;
    // appModel->findClipById(piece->clip->id(), track);
    // auto trackContext = AudioContext::instance()->getContextFromTrack(track);
    // auto convertTime = trackContext->projectContext()->timeConverter();
    // auto startSample = convertTime(piece->realStartTick());
    // clipContext.clipSeries->setClipStartPos(clipContext.clipView, startSample);
    // auto firstSample = convertTime(piece->realStartTick());
    // auto lastSample = convertTime(piece->realEndTick() - piece->realStartTick());
    // clipContext.clipSeries->setClipRange(clipContext.clipView, firstSample,
    //                                      qMax(qint64(1), lastSample - firstSample));

    connect(piece, &InferPiece::statusChanged, this,
            [=](InferStatus status) { handlePieceStatusChanged(status, piece); });
}

void PieceController::removePiece(InferPiece *piece){

}
