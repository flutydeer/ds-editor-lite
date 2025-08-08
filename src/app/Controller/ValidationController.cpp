//
// Created by fluty on 2024/7/17.
//

#include "ValidationController.h"

#include "PlaybackController.h"
#include "Model/AppModel/Track.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Toast.h"
#include "Utils/G2pUtil.h"

// TODO: 调整逻辑避免不必要的 validation
ValidationController::ValidationController() {
    // qDebug() << "ValidationController::ValidationController";
    connect(appModel, &AppModel::modelChanged, this, &ValidationController::onModelChanged);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            &ValidationController::onUndoRedoChanged);
}

void ValidationController::runValidation() {
    validate();
}

void ValidationController::onUndoRedoChanged() {
    // validate();
}

void ValidationController::onModelChanged() {
    // qDebug() << "ValidationController::onModelChanged";
    for (const auto track : m_tracks)
        onTrackChanged(AppModel::Remove, -1, track);

    for (const auto track : appModel->tracks()) {
        // Set track default language
        track->setDefaultLanguage(appOptions->general()->defaultSingingLanguage);
        // TODO: Temp Use
        track->setDefaultG2pId(defaultG2pId());
        onTrackChanged(AppModel::Insert, -1, track);
        connect(track, &Track::clipChanged, this, &ValidationController::onClipChanged);
        for (const auto clip : track->clips()) {
            // m_clips.append(clip);
            handleClipInserted(clip);
            if (clip->clipType() == Clip::Singing) {
                const auto singingClip = reinterpret_cast<SingingClip *>(clip);
                singingClip->defaultLanguage = track->defaultLanguage();
                singingClip->defaultG2pId = track->defaultG2pId();
                singingClip->configPath = appOptions->general()->defaultPackage;
            }
        }
    }
    validate();
}

void ValidationController::onTempoChanged(double tempo) {
    // qDebug() << "ValidationController::onTempoChanged" << tempo;
    // terminate all validate tasks
    validate();
}

void ValidationController::onTrackChanged(const AppModel::TrackChangeType type, qsizetype index,
                                          Track *track) {
    // qDebug() << "ValidationController::onTrackChanged" << type;
    if (type == AppModel::Insert) {
        m_tracks.append(track);
        connect(track, &Track::clipChanged, this, &ValidationController::onClipChanged);
    } else if (type == AppModel::Remove) {
        m_tracks.removeOne(track);
        // for (const auto clip : track->clips())
        //     handleClipRemoved(clip);
        disconnect(track, &Track::clipChanged, this, &ValidationController::onClipChanged);
    }
    validate();
}

void ValidationController::onClipChanged(const Track::ClipChangeType type, Clip *clip) {
    // qDebug() << "ValidationController::onClipChanged" << type;
    if (type == Track::Inserted) {
        handleClipInserted(clip);
    } else if (type == Track::Removed) {
        m_clips.removeOne(clip);
        disconnect(clip, &Clip::propertyChanged, this, nullptr);
    }
    validate();
}

void ValidationController::onClipPropertyChanged(Clip *clip) {
    // qDebug() << "ValidationController::onClipPropertyChanged";
    if (!m_clips.contains(clip))
        return;

    validate();
}

void ValidationController::onNoteChanged(const SingingClip::NoteChangeType type,
                                         const QList<Note *> &notes) {
    // qDebug() << "onNoteChanged";
    switch (type) {
        case SingingClip::Insert:
        case SingingClip::Remove:
        case SingingClip::TimeKeyPropertyChange:
            validate();
            break;
        default:
            break;
    }
}

void ValidationController::handleClipInserted(Clip *clip) {
    m_clips.append(clip);
    connect(clip, &Clip::propertyChanged, this, [clip, this] { onClipPropertyChanged(clip); });

    if (clip->clipType() == Clip::Singing) {
        const auto singingClip = reinterpret_cast<SingingClip *>(clip);
        connect(singingClip, &SingingClip::noteChanged, this, &ValidationController::onNoteChanged);
    }
    validate();
}

void ValidationController::validate() {
    // qDebug() << "ValidationController::validate";
    if (!validateProjectLength() || !validateTempo() || !validateNoteOverlap()) {
        emit validationFinished(false);
        playbackController->stop();
    } else {
        emit validationFinished(true);
    }
}

bool ValidationController::validateProjectLength() {
    const auto length = appModel->tickToMs(appModel->projectLengthInTicks());
    if (length > 30 * 60 * 1000) { // > 30 min
        Toast::show("Project is too long");
        return false;
    }
    return true;
}

bool ValidationController::validateTempo() {
    // 用于测试
    const auto tempo = appModel->tempo();
    if (tempo < 5) {
        Toast::show("Tempo is too slow");
        return false;
    }
    return true;
}

bool ValidationController::validateNoteOverlap() {
    for (const auto track : appModel->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() == Clip::Singing) {
                const auto singingClip = reinterpret_cast<SingingClip *>(clip);
                if (singingClip->notes().hasOverlappedItem()) {
                    Toast::show("Note overlapped");
                    return false;
                }
            }
        }
    }
    return true;
}