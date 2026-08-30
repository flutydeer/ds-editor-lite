#include "MoveTrackAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

MoveTrackAction *MoveTrackAction::build(const qsizetype fromIndex, const qsizetype toIndex,
                                        AppModel *model) {
    const auto a = new MoveTrackAction;
    a->m_fromIndex = fromIndex;
    a->m_toIndex = toIndex;
    a->m_model = model;
    return a;
}

void MoveTrackAction::execute() {
    if (!m_track) {
        const auto size = m_model->tracks().size();
        if (m_fromIndex < 0 || m_fromIndex >= size || m_toIndex < 0 || m_toIndex > size)
            return;

        // 拖拽给出的 toIndex 是「插入到该位置之前」的语义；moveTrack 需要的是移动后的
        // 最终下标，因此当目标位于源之后时要向前修正一位。
        m_actualToIndex = m_toIndex > m_fromIndex ? m_toIndex - 1 : m_toIndex;
        if (m_fromIndex == m_actualToIndex)
            return;
        m_track = m_model->tracks().at(m_fromIndex);
    }

    const auto currentIndex = m_model->tracks().indexOf(m_track);
    if (currentIndex < 0 || currentIndex == m_actualToIndex)
        return;
    if (m_model->moveTrack(currentIndex, m_actualToIndex))
        m_model->notifyTrackMoved(currentIndex, m_actualToIndex);
}

void MoveTrackAction::undo() {
    if (!m_track || m_actualToIndex < 0)
        return;

    const auto currentIndex = m_model->tracks().indexOf(m_track);
    if (currentIndex < 0 || currentIndex == m_fromIndex)
        return;
    if (m_model->moveTrack(currentIndex, m_fromIndex))
        m_model->notifyTrackMoved(currentIndex, m_fromIndex);
}
