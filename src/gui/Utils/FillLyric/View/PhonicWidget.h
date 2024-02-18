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

#include "PhonicDelegate.h"
#include "PhonicTextEdit.h"
#include "PhonicEventFilter.h"

namespace FillLyric {
    using PhonicRole = PhonicDelegate::PhonicRole;
    using LyricType = CleanLyric::LyricType;

    class PhonicWidget : public QWidget {
        Q_OBJECT
    public:
        explicit PhonicWidget(QObject *parent = nullptr);
        ~PhonicWidget() override;

        void setLyrics(QList<QList<QString>> &lyrics);
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

        void _on_btnToText_clicked();
        void _on_btnToTable_clicked();
        void _on_btnToggleFermata_clicked();
        void _on_showContextMenu(const QPoint &pos);

        void _on_changePhonetic(const QModelIndex &index, QMenu *menu);
        void _on_changeSyllable(const QModelIndex &index, QMenu *menu);

        void _on_textEditChanged();
        void _on_modelDataChanged();

        void _on_btnInsertText_clicked();
        void _on_btnImportLrc_clicked();

    private:
        void _init(QList<QList<QString>> lyricRes);
        void resizeTable();
        QList<Phonic> updateLyric(QModelIndex index, const QString &text,
                                  const QList<Phonic> &oldPhonics);

        // Variables
        int notesCount = 0;
        int maxLyricLength = 0;
        int maxSyllableLength = 0;

        PhonicTextEdit *textEdit;
        QVBoxLayout *cfgLayout;

        PhonicDelegate *delegate;
        PhonicEventFilter *eventFilter;
        QTableView *tableView;

        // Buttons
        QPushButton *btnInsertText;
        QPushButton *btnToTable;
        QPushButton *btnToText;
        QPushButton *btnToggleFermata;
        QPushButton *btnImportLrc;

        QPushButton *btnUndo;
        QPushButton *btnRedo;

        // labels
        QLabel *textCountLabel;
        QLabel *noteCountLabel;

        // Model
        PhonicModel *model;
        IKg2p::Mandarin g2p_man;
        IKg2p::JpG2p g2p_jp;

        // Layout
        QHBoxLayout *topLayout;
        QHBoxLayout *tableLayout;
        QVBoxLayout *mainLayout;
    };
}

#endif // DS_EDITOR_LITE_PHONICWIDGET_H
