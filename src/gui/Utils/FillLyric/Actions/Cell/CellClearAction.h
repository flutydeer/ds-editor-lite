#ifndef DS_EDITOR_LITE_CELLCLEARACTION_H
#define DS_EDITOR_LITE_CELLCLEARACTION_H

#include <QObject>
#include <QModelIndex>

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

#include "../../Utils/CleanLyric.h"
#include "../../View/PhonicDelegate.h"

namespace FillLyric {
    using LyricType = CleanLyric::LyricType;
    using PhonicRole = PhonicDelegate::PhonicRole;

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
        LyricType m_type = LyricType::Other;
        bool m_lineFeed = false;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_CELLCLEARACTION_H
