#include "EditTemposAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <utility>

namespace {
    class ClipGroupStates {
    public:
        SingingClip *clip = nullptr;
        SingingClipPhonemeNormalizer::GroupStates states;
    };

    QList<ClipGroupStates> captureGroupStates(AppModel &model) {
        QList<ClipGroupStates> result;
        const auto &timeline = model.timeline();
        for (const auto track : model.tracks()) {
            for (const auto clip : track->clips()) {
                if (clip->clipType() != IClip::Singing)
                    continue;
                auto singingClip = static_cast<SingingClip *>(clip);
                auto states =
                    SingingClipPhonemeNormalizer::captureGroupStates(*singingClip, timeline);
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
    return a;
}

void EditTemposAction::execute() {
    const auto previousGroupStates = captureGroupStates(*m_model);
    auto timeline = m_model->timeline();
    timeline.setTempos(m_newTempos);
    m_model->setTimeline(std::move(timeline));

    m_resetRecords.clear();
    for (const auto &previous : previousGroupStates) {
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
    for (const auto &reset : m_resetRecords) {
        SingingClipPhonemeNormalizer::restoreEditedOffsets(reset.records);
        reset.clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(reset.records));
    }
}
