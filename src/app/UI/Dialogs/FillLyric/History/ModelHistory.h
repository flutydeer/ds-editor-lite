#ifndef DS_EDITOR_LITE_MODELHISTORY_H
#define DS_EDITOR_LITE_MODELHISTORY_H

#include <QObject>
#include <QStack>

#include "MActionSequence.h"
#include "Utils/Singleton.h"

namespace FillLyric {

    class ModelHistory final : public QObject, public Singleton<ModelHistory> {
        Q_OBJECT
    public:
        void undo();
        void redo();
        void record(MActionSequence *actions);
        void reset();

        bool canUndo() const;
        bool canRedo() const;

    signals:
        void undoRedoChanged(bool canUndo, bool canRedo);

    private:
        QStack<MActionSequence *> m_undoStack;
        QStack<MActionSequence *> m_redoStack;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_MODELHISTORY_H
