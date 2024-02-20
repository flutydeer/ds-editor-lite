#ifndef DS_EDITOR_LITE_PHONICWIDGET_H
#define DS_EDITOR_LITE_PHONICWIDGET_H

#include <QApplication>
#include <QPainter>
#include <QMenu>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextEdit>
#include <QLabel>
#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QAbstractButton>

#include "g2pglobal.h"
#include "mandarin.h"
#include "jpg2p.h"

#include "../Utils/CleanLyric.h"
#include "../Model/PhonicModel.h"
#include "../History/ModelHistory.h"
#include "Controls/Button.h"

#include "PhonicDelegate.h"
#include "PhonicTextEdit.h"
#include "PhonicEventFilter.h"

namespace FillLyric {
    class PhonicWidget : public QWidget {
        Q_OBJECT
        friend class LyricWidget;

    public:
        explicit PhonicWidget(QList<PhonicNote *> phonicNotes, QWidget *parent = nullptr);
        ~PhonicWidget() override;

        QList<Phonic> exportPhonics();

    public Q_SLOTS:
        // ContextMenu
        void cellClear(const QList<QModelIndex> &indexes);
        void deleteCell(const QModelIndex &index);
        void insertCell(const QModelIndex &index);
        void cellMergeLeft(const QModelIndex &index);
        void cellChangePhonic(const QModelIndex &index, const QString &syllableRevised);

        // Line Operations
        void lineBreak(QModelIndex index);
        void addPrevLine(QModelIndex index);
        void addNextLine(QModelIndex index);
        void removeLine(QModelIndex index);
        void lineMergeUp(QModelIndex index);

        void _on_cellEditClosed(QModelIndex index, const QString &text);
        void _on_showContextMenu(const QPoint &pos);

        void _on_changePhonetic(const QModelIndex &index, QMenu *menu);
        void _on_changeSyllable(const QModelIndex &index, QMenu *menu);

        void _on_btnToggleFermata_clicked();

    protected:
        QTableView *tableView;
        PhonicModel *model;

    private:
        void _init(const QList<Phonic>& phonics);
        void resizeTable();
        QList<Phonic> updateLyric(QModelIndex index, const QString &text,
                                  const QList<Phonic> &oldPhonics);

        // Variables
        QList<PhonicNote *> m_phonicNotes;

        int maxLyricLength = 0;
        int maxSyllableLength = 0;

        PhonicDelegate *delegate;
        PhonicEventFilter *eventFilter;

        // Model
        IKg2p::Mandarin g2p_man;
        IKg2p::JpG2p g2p_jp;

        // Layout
        QVBoxLayout *mainLayout;
    };
}

#endif // DS_EDITOR_LITE_PHONICWIDGET_H
