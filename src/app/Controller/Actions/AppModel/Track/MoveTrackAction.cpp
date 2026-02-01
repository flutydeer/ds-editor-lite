//
// Created by fluty on 2024/7/11.
//

#include "MoveTrackAction.h"

#include "Model/AppModel/AppModel.h"

MoveTrackAction *MoveTrackAction::build(const qsizetype fromIndex, const qsizetype toIndex,
                                         AppModel *model) {
    const auto a = new MoveTrackAction;
    a->m_fromIndex = fromIndex;
    a->m_toIndex = toIndex;
    a->m_model = model;
    return a;
}

void MoveTrackAction::execute() {
    if (m_fromIndex == m_toIndex)
        return;

    const auto &tracks = m_model->tracks();
    if (m_fromIndex < 0 || m_fromIndex >= tracks.size() || m_toIndex < 0 ||
        m_toIndex > tracks.size())
        return;

    auto *track = tracks[m_fromIndex];
    m_model->removeTrackAt(m_fromIndex);

    // After removal, if toIndex is beyond the removed item, we need to adjust
    m_actualToIndex = m_toIndex;
    if (m_toIndex > m_fromIndex) {
        m_actualToIndex = m_toIndex - 1;
    }

    m_model->insertTrack(track, m_actualToIndex);
}

void MoveTrackAction::undo() {
    if (m_fromIndex == m_toIndex)
        return;

    const auto &tracks = m_model->tracks();
    if (m_actualToIndex < 0 || m_actualToIndex >= tracks.size())
        return;

    auto *track = tracks[m_actualToIndex];
    m_model->removeTrackAt(m_actualToIndex);

    // After removal, m_fromIndex should be valid for insertion
    // (can be up to the size of the shortened list)
    if (m_fromIndex < 0 || m_fromIndex > tracks.size() - 1)
        return;

    m_model->insertTrack(track, m_fromIndex);
}
