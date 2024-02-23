#ifndef DS_EDITOR_LITE_CELLCLEARACTION_H
#define DS_EDITOR_LITE_CELLCLEARACTION_H

#include <QObject>
#include <QModelIndex>

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

#include "../../Model/PhonicCommon.h"

namespace FillLyric {
    class CellClearAction final : public MAction {
    public:
        static CellClearAction *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_index;

        QString m_lyric;
        QString m_syllable;
        QStringList m_candidates;
        QString m_syllableRevised;
        TextType m_type = TextType::Other;
        bool m_lineFeed = false;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_CELLCLEARACTION_H
