//
// Created by fluty on 2024/7/17.
//

#include "ValidationController.h"

#include "Model/AppModel/Track.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Toast.h"
#include "Utils/NoteWordUtils.h"

ValidationController::ValidationController() {
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
    qDebug() << "ValidationController::onModelChanged";
    m_tracks.clear();

    for (const auto track : appModel->tracks()) {
        // Set track default language
        track->setDefaultLanguage(appOptions->general()->defaultSingingLanguage);
        // m_tracks.append(track);
        // connect(track, &Track::clipChanged, this, &ValidationController::onClipChanged);
        for (const auto clip : track->clips()) {
            // m_clips.append(clip);
            // connect(clip, &Clip::propertyChanged, this, [=] { onClipPropertyChanged(clip); });
            if (clip->clipType() == Clip::Singing) {
                auto singingClip = reinterpret_cast<SingingClip *>(clip);
                singingClip->defaultLanguage = track->defaultLanguage();
                // NoteWordUtils::updateOriginalWordProperties(singingClip->notes().toList());
                // if (appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready)
                //     NoteWordUtils::updateOriginalWordProperties(singingClip->notes().toList());
                // else
                //     m_notesPendingUpdateNoteWordProperty.append(singingClip->notes().toList());
            }
        }
    }
    validate();
}

void ValidationController::onTempoChanged(double tempo) {
    qDebug() << "ValidationController::onTempoChanged" << tempo;
    // terminate all validate tasks
    validate();
}

void ValidationController::onTrackChanged(AppModel::TrackChangeType type, qsizetype index,
                                          Track *track) {
    qDebug() << "ValidationController::onTrackChanged" << type;
    if (type == AppModel::Insert) {
        m_tracks.append(track);
        connect(track, &Track::clipChanged, this, &ValidationController::onClipChanged);
    } else if (type == AppModel::Remove) {
        m_tracks.removeOne(track);
        disconnect(track, &Track::clipChanged, this, &ValidationController::onClipChanged);
    }
    validate();
}

void ValidationController::onClipChanged(Track::ClipChangeType type, Clip *clip) {
    qDebug() << "ValidationController::onClipChanged" << type;
    if (type == Track::Inserted) {
        handleClipInserted(clip);
    } else if (type == Track::Removed) {
        m_clips.removeOne(clip);
        disconnect(clip, &Clip::propertyChanged, this, nullptr);
    }
    validate();
}

void ValidationController::onClipPropertyChanged(Clip *clip) {
    qDebug() << "ValidationController::onClipPropertyChanged";
    if (!m_clips.contains(clip))
        return;

    validate();
}

void ValidationController::onNoteChanged(SingingClip::NoteChangeType type, Note *note) {
    qDebug() << "ValidationController::onNoteChanged";
    if (type == SingingClip::Inserted)
        handleNoteInserted(note);
    validate();
}

void ValidationController::handleClipInserted(Clip *clip) {
    m_clips.append(clip);
    connect(clip, &Clip::propertyChanged, this, [=] { onClipPropertyChanged(clip); });

    if (clip->clipType() == Clip::Singing) {
        auto singingClip = reinterpret_cast<SingingClip *>(clip);
        connect(singingClip, &SingingClip::noteChanged, this, &ValidationController::onNoteChanged);
    }
}

void ValidationController::handleNoteInserted(Note *note) {
    handleNotePropertyChanged(Note::Word, note);
    connect(note, &Note::propertyChanged, this,
            [=](Note::NotePropertyType type) { handleNotePropertyChanged(type, note); });
}

void ValidationController::handleNotePropertyChanged(Note::NotePropertyType type, Note *note) {
    qDebug() << "ValidationController::handleNotePropertyChanged";
    if (type == Note::Word) {
        // if (appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready)
        //     NoteWordUtils::updateOriginalWordProperties(note->clip()->notes().toList());
        // else
        //     m_notesPendingUpdateNoteWordProperty.append(note->clip()->notes().toList());
    }
}

void ValidationController::validate() {
    qDebug() << "ValidationController::validate";
    if (!validateProjectLength() || !validateTempo() || !validateClipOverlap() ||
        !validateNoteOverlap()) {
        emit validationFinished(false);
    } else {
        emit validationFinished(true);
    }
}

bool ValidationController::validateProjectLength() {
    auto length = appModel->tickToMs(appModel->projectLengthInTicks());
    if (length > 30 * 60 * 1000) { // > 30 min
        Toast::show("ValidationController: project is too long");
        return false;
    }
    return true;
}

bool ValidationController::validateTempo() {
    // 用于测试
    auto tempo = appModel->tempo();
    if (tempo < 5) {
        Toast::show("ValidationController: tempo is too slow");
        return false;
    }
    return true;
}

bool ValidationController::validateClipOverlap() {
    if (std::all_of(appModel->tracks().begin(), appModel->tracks().end(),
                    [](const auto &track) { return !track->clips().hasOverlappedItem(); })) {
        return true;
    }
    Toast::show("ValidationController: clip overlapped");
    return false;
}

bool ValidationController::validateNoteOverlap() {
    for (const auto track : appModel->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() == Clip::Singing) {
                auto singingClip = reinterpret_cast<SingingClip *>(clip);
                if (singingClip->notes().hasOverlappedItem()) {
                    Toast::show("ValidationController: note overlapped");
                    return false;
                }
            }
        }
    }
    return true;
}