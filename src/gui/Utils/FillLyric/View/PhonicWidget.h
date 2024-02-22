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
#include "Utils/G2p/G2pMandarin.h"
#include "Utils/G2p/G2pJapanese.h"

#include "../Utils/CleanLyric.h"
#include "../Model/PhonicModel.h"
#include "../History/ModelHistory.h"
#include "Controls/Button.h"

#include "PhonicDelegate.h"
#include "PhonicTableView.h"
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

    Q_SIGNALS:
        void historyReset();

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

        void setAutoWrap(bool wrap);
        void setFontSizeDiff(int diff);
        void setColWidthRatio(double ratio);
        void setRowHeightRatio(double ratio);

    protected:
        PhonicTableView *tableView;
        PhonicModel *model;

    private:
        void _init(const QList<Phonic> &phonics);
        void autoWrap();
        void resizeTable();
        QList<Phonic> updateLyric(QModelIndex index, const QString &text,
                                  const QList<Phonic> &oldPhonics);

        // Variables
        QList<PhonicNote *> m_phonicNotes;

        bool autoWarp = false;

        int fontSizeDiff = 3;
        double rowHeightRatio = 3.0;
        double colWidthRatio = 7.8;

        PhonicDelegate *delegate;
        PhonicEventFilter *eventFilter;

        // Model
        G2pMandarin *g2p_man;
        G2pJapanese *g2p_jp;

        // Layout
        QVBoxLayout *mainLayout{};
    };
}

#endif // DS_EDITOR_LITE_PHONICWIDGET_H
