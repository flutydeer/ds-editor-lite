//
// Created by fluty on 2024/7/17.
//

#include "ValidationController.h"

#include "Model/Track.h"
#include "UI/Controls/Toast.h"

ValidationController::ValidationController() {
    auto model = AppModel::instance();
    connect(model, &AppModel::modelChanged, this, &ValidationController::onModelChanged);
    connect(model, &AppModel::tempoChanged, this, &ValidationController::onTempoChanged);
    connect(model, &AppModel::trackChanged, this, &ValidationController::onTrackChanged);
}
void ValidationController::runValidation() {
    validate();
}
void ValidationController::onModelChanged() {
    qDebug() << "ValidationController::onModelChanged";
    for (auto track : m_tracks)
        disconnect(track, &Track::clipChanged, this, &ValidationController::onClipChanged);
    m_tracks.clear();

    for (const auto track : AppModel::instance()->tracks()) {
        m_tracks.append(track);
        connect(track, &Track::clipChanged, this, &ValidationController::onClipChanged);

        for(const auto clip : track->clips()) {
            m_clips.append(clip);
            connect(clip, &Clip::propertyChanged, this, [=] { onClipPropertyChanged(clip); });
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
void ValidationController::onClipChanged(Track::ClipChangeType type, int id, Clip *clip) {
    qDebug() << "ValidationController::onClipChanged" << type;
    if (type == Track::Inserted) {
        m_clips.append(clip);
        connect(clip, &Clip::propertyChanged, this, [=] { onClipPropertyChanged(clip); });
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
void ValidationController::validate() {
    qDebug() << "ValidationController::validate";
    if (!validateProjectLength() || !validateTempo() || !validateClipOverlap()) {
        emit validationFinished(false);
    } else {
        emit validationFinished(true);
    }
}
bool ValidationController::validateProjectLength() {
    auto length = AppModel::instance()->tickToMs(AppModel::instance()->projectLengthInTicks());
    if (length > 30 * 60 * 1000) { // > 30 min
        Toast::show("ValidationController: project is too long");
        return false;
    }
    return true;
}
bool ValidationController::validateTempo() {
    // 用于测试
    auto tempo = AppModel::instance()->tempo();
    if (tempo < 5) {
        Toast::show("ValidationController: tempo is too slow");
        return false;
    }
    return true;
}
bool ValidationController::validateClipOverlap() {
    if (std::all_of(AppModel::instance()->tracks().begin(), AppModel::instance()->tracks().end(),
                    [](const auto &track) { return !track->clips().isOverlappedItemExists(); })) {
        return true;
    }
    Toast::show("ValidationController: clip overlapped");
    return false;
}
bool ValidationController::validateNoteOverlap() {
    return true;
}