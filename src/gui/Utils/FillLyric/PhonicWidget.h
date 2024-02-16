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
#include "Utils/CleanLyric.h"

#include "mandarin.h"
#include "jpg2p.h"
#include "PhonicDelegate.h"
#include "PhonicModel.h"
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

        void setLyrics(const QString &lyrics);

    private:
        void resizeTable();

        void _on_btnToText_clicked();
        void _on_btnToTable_clicked();
        void _on_btnToggleFermata_clicked();
        void _on_showContextMenu(const QPoint &pos);

        void _on_changePhonetic(const QModelIndex &index, QMenu *menu);
        void _on_changeSyllable(const QModelIndex &index, QMenu *menu);

        void _on_cellEditClosed();

        void _on_btnInsertText_clicked();
        void _on_btnImportLrc_clicked();
        void _on_btnExport_clicked();

        int maxLyricLength = 0;
        int maxSyllableLength = 0;

        PhonicTextEdit *textEdit;
        QVBoxLayout *cfgLayout;
        PhonicEventFilter *eventFilter;
        QTableView *tableView;

        QPushButton *btnInsertText;
        QPushButton *btnToTable;
        QPushButton *btnToText;
        QPushButton *btnToggleFermata;
        QPushButton *btnImportLrc;

        QPushButton *btnExport;
        QPushButton *btnCancel;

        PhonicModel *model;
        IKg2p::Mandarin g2p_man;
        IKg2p::JpG2p g2p_jp;

        QHBoxLayout *tableLayout;
        QHBoxLayout *bottomLayout;
        QVBoxLayout *mainLayout;
    };
}

#endif // DS_EDITOR_LITE_PHONICWIDGET_H
