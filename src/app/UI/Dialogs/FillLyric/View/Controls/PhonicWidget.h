#ifndef DS_EDITOR_LITE_PHONICWIDGET_H
#define DS_EDITOR_LITE_PHONICWIDGET_H

#include <QWidget>

#include "g2pglobal.h"
#include "Modules/Language/G2pMgr/IG2pManager.h"

#include "../../Utils/CleanLyric.h"
#include "../../Model/PhonicModel.h"

#include "PhonicTableView.h"
#include "PhonicEventFilter.h"

namespace FillLyric {
    class PhonicWidget final : public QWidget {
        Q_OBJECT
        friend class LyricTab;
        friend class LyricExtWidget;

    public:
        explicit PhonicWidget(QWidget *parent = nullptr);
        ~PhonicWidget() override;

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

    protected:
        PhonicTableView *tableView;
        PhonicModel *model;

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
        G2pMgr::IG2pManager *g2pMgr;

        // Layout
        QVBoxLayout *mainLayout{};
    };
}

#endif // DS_EDITOR_LITE_PHONICWIDGET_H
