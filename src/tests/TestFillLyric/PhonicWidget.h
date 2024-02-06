#ifndef DS_EDITOR_LITE_PHONICWIDGET_H
#define DS_EDITOR_LITE_PHONICWIDGET_H

#include <QApplication>
#include <QPainter>
#include <QMenu>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextEdit>
#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QAbstractButton>

#include "g2pglobal.h"
#include "CleanLyric.h"

#include "mandarin.h"
#include "SyllableDelegate.h"

namespace FillLyric {
    class PhonicWidget : public QWidget {
        Q_OBJECT
    public:
        explicit PhonicWidget(QObject *parent = nullptr);
        ~PhonicWidget() override;

    private:
        int currentLyricLength(int row);

        void _on_btnToText_clicked();
        void _on_btnToTable_clicked();
        void _on_showContextMenu(const QPoint &pos);

        void _on_cellClear(const QModelIndex &index);
        void _on_cellMoveLeft(const QModelIndex &index);
        void _on_cellMoveRight(const QModelIndex &index);
        void _on_cellNewLine(const QModelIndex &index);
        void _on_cellMergeUp(const QModelIndex &index);
        void _on_changeSyllable(const QModelIndex &index, QMenu *menu);

        void _on_cellChanged(const QModelIndex &index);

        void _on_btnInsertText_clicked();
        void _on_btnExport_clicked();

        void _on_cellEditClosed();


        QTextEdit *textEdit;
        QVBoxLayout *cfgLayout;
        QTableView *tableView;

        QPushButton *btnInsertText;
        QPushButton *btnToTable;
        QPushButton *btnToText;

        QPushButton *btnExport;
        QPushButton *btnCancel;

        int modelMaxCol = 0;
        QStandardItemModel *model;
        IKg2p::Mandarin g2p_man;

        QHBoxLayout *tableLayout;
        QHBoxLayout *bottomLayout;
        QVBoxLayout *mainLayout;
    };
}

#endif // DS_EDITOR_LITE_PHONICWIDGET_H
