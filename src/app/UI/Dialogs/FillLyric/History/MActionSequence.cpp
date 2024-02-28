#include "MActionSequence.h"

namespace FillLyric {
    void MActionSequence::execute() {
        for (auto action : m_actionSequence) {
            action->execute();
        }
    }
    void MActionSequence::undo() {
        for (auto action : m_actionSequence) {
            action->undo();
        }
    }
    qsizetype MActionSequence::count() {
        return m_actionSequence.count();
    }
    void MActionSequence::addAction(MAction *action) {
        m_actionSequence.append(action);
    }
} // FillLyric