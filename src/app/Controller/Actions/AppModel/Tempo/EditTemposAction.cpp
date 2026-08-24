#include "EditTemposAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <utility>

namespace {
    class ClipWordStates {
    public:
        SingingClip *clip = nullptr;
        SingingClipPhonemeNormalizer::WordStates states;
    };

    QList<ClipWordStates> captureWordStates(AppModel &model) {
        QList<ClipWordStates> result;
        const auto &timeline = model.timeline();
        for (const auto track : model.tracks()) {
            for (const auto clip : track->clips()) {
                if (clip->clipType() != IClip::Singing)
                    continue;
                auto singingClip = static_cast<SingingClip *>(clip);
                auto states =
                    SingingClipPhonemeNormalizer::captureWordStates(*singingClip, timeline);
                if (!states.isEmpty())
                    result.append({singingClip, std::move(states)});
            }
        }
        return result;
    }
}

EditTemposAction *EditTemposAction::build(const QList<Tempo> &oldTempos,
                                          const QList<Tempo> &newTempos, AppModel *model) {
    const auto a = new EditTemposAction;
    a->m_oldTempos = oldTempos;
    a->m_newTempos = newTempos;
    a->m_model = model;
    for (const auto track : model->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() != IClip::Audio)
                continue;
            const auto audioClip = static_cast<AudioClip *>(clip);
            a->m_audioClipTicks.append({track, audioClip, audioClip->start(), audioClip->length(),
                                        audioClip->clipStart(), audioClip->clipLen()});
        }
    }
    return a;
}

void EditTemposAction::execute() {
    const auto previousWordStates = captureWordStates(*m_model);
    auto timeline = m_model->timeline();
    timeline.setTempos(m_newTempos);
    m_model->setTimeline(std::move(timeline));

    m_resetRecords.clear();
    for (const auto &previous : previousWordStates) {
        auto records = SingingClipPhonemeNormalizer::normalizeEditedOffsets(
            *previous.clip, previous.states, m_model->timeline());
        if (records.isEmpty())
            continue;
        m_resetRecords.append({previous.clip, std::move(records)});
        previous.clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords.last().records));
    }
}

void EditTemposAction::undo() {
    auto timeline = m_model->timeline();
    timeline.setTempos(m_oldTempos);
    m_model->setTimeline(std::move(timeline));
    for (const auto &snapshot : m_audioClipTicks) {
        const bool changed = snapshot.clip->start() != snapshot.start ||
                             snapshot.clip->length() != snapshot.length ||
                             snapshot.clip->clipStart() != snapshot.clipStart ||
                             snapshot.clip->clipLen() != snapshot.clipLen;
        if (!changed)
            continue;
        snapshot.track->removeClip(snapshot.clip);
        snapshot.clip->setStart(snapshot.start);
        snapshot.clip->setLength(snapshot.length);
        snapshot.clip->setClipStart(snapshot.clipStart);
        snapshot.clip->setClipLen(snapshot.clipLen);
        snapshot.track->insertClip(snapshot.clip);
        snapshot.clip->notifyPropertyChanged();
    }
    for (const auto &reset : m_resetRecords) {
        SingingClipPhonemeNormalizer::restoreEditedOffsets(reset.records);
        reset.clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(reset.records));
    }
}
