#ifndef DS_EDITOR_LITE_PHONICTABLEVIEW_H
#define DS_EDITOR_LITE_PHONICTABLEVIEW_H

#include <QTableView>
#include "Modules/Language/LangMgr/ILanguageManager.h"

#include "../../Utils/SplitLyric.h"
#include "../../Model/PhonicModel.h"

#include "PhonicTableView.h"
#include "PhonicEventFilter.h"

namespace FillLyric {

    class PhonicTableView final : public QTableView {
        Q_OBJECT
        friend class LyricTab;
        friend class LyricExtWidget;

    public:
        explicit PhonicTableView(QWidget *parent = nullptr);
        ~PhonicTableView() override;

    Q_SIGNALS:
        void sizeChanged() const;

    protected:
        void resizeEvent(QResizeEvent *event) override;
        PhonicModel *model;

    public Q_SLOTS:
        // ContextMenu
        void cellClear(const QList<QModelIndex> &indexes) const;
        void deleteCell(const QModelIndex &index) const;
        void insertCell(const QModelIndex &index) const;
        void cellMergeLeft(const QModelIndex &index) const;
        void cellChangePhonic(const QModelIndex &index, const QString &syllableRevised) const;

        // Line Operations
        void lineBreak(const QModelIndex &index) const;
        void addPrevLine(const QModelIndex &index) const;
        void addNextLine(const QModelIndex &index) const;
        void removeLine(const QModelIndex &index) const;
        void lineMergeUp(const QModelIndex &index) const;

        void _on_cellEditClosed(const QModelIndex &index, const QString &text) const;
        void _on_setToolTip(const QModelIndex &index) const;
        void _on_clearToolTip(const QModelIndex &index) const;
        void _on_showContextMenu(const QPoint &pos);

        void _on_changePhonetic(const QModelIndex &index, QMenu *menu);
        void _on_changeSyllable(const QModelIndex &index, QMenu *menu) const;

        void _on_btnToggleFermata_clicked() const;

        void setAutoWrap(const bool &wrap);
        void setColWidthRatio(double ratio);
        void setRowHeightRatio(const double &ratio);

    Q_SIGNALS:
        void historyReset();

    private:
        void _init(const QList<Phonic> &phonics);
        void tableAutoWrap(const bool &switchState = false) const;
        void resizeTable() const;
        [[nodiscard]] QList<Phonic> updateLyric(const QModelIndex &index, const QString &text,
                                                const QList<Phonic> &oldPhonics) const;

        // Variables
        QList<Phonic *> m_phonics;

        bool autoWrap = false;

        double rowHeightRatio = 3.0;
        double colWidthRatio = 7.8;

        PhonicDelegate *delegate;
        PhonicEventFilter *eventFilter;

        // Model
        LangMgr::ILanguageManager *langMgr;

        // Layout
        QVBoxLayout *mainLayout{};
    };

} // FillLyric

#endif // DS_EDITOR_LITE_PHONICTABLEVIEW_H
