#ifndef DS_EDITOR_LITE_PHONICMODEL_H
#define DS_EDITOR_LITE_PHONICMODEL_H
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
#include "PhonicDelegate.h"


namespace FillLyric {
    using PhonicRole = PhonicDelegate::PhonicRole;
    using LyricType = CleanLyric::LyricType;

    class PhonicModel : public QStandardItemModel {
    public:
        void shrinkModel();
        void repaintTable();

        static QList<int> displayRole();
        static QList<int> allRoles();
        int cellLyricType(int row, int col);
        QString cellLyric(int row, int col);
        int currentLyricLength(int row);

        void collapseFermata();
        void expandFermata();
        void setFermata(int row, int col, QString &fermata);
        void clearData(int row, int col, const QList<int> &roles);
        void moveData(int row, int col, int tarRow, int tarCol, const QList<int> &roles);

        void cellClear(const QModelIndex &index);
        void cellMergeLeft(const QModelIndex &index);
        void cellMoveLeft(const QModelIndex &index);
        void cellMoveRight(const QModelIndex &index);
        void cellNewLine(const QModelIndex &index);
        void cellMergeUp(const QModelIndex &index);

        void addPrevLine(const QModelIndex &index);
        void addNextLine(const QModelIndex &index);
        void removeLine(const QModelIndex &index);

        bool fermataState = false;

        int modelMaxCol = 0;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_PHONICMODEL_H
