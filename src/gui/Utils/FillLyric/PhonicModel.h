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
        explicit PhonicModel(QTableView *tableView, QObject *parent = nullptr)
            : QStandardItemModel(parent), tableView(tableView) {
        }

        // Gui functions
        void repaintView();
        void shrinkModel();
        void expandModel(int col);

        // Basic functions
        int currentLyricLength(int row);

        // RoleData functions
        static QList<int> allRoles();

        QString cellLyric(int row, int col);
        bool setLyric(int row, int col, QString &lyric);
        QString cellSyllable(int row, int col);
        bool setSyllable(int row, int col, const QString &syllable);
        QStringList cellCandidate(int row, int col);
        bool setCandidate(int row, int col, const QStringList& candidate);
        int cellLyricType(int row, int col);
        bool setLyricType(int row, int col, LyricType type);
        QStringList cellFermata(int row, int col);
        bool setFermata(int row, int col, const QList<QString> &fermata);

        // Cell operations
        void clearData(int row, int col, const QList<int> &roles);
        void moveData(int row, int col, int tarRow, int tarCol, const QList<int> &roles);

        void cellClear(const QModelIndex &index);
        void cellMergeLeft(const QModelIndex &index);
        void cellMoveLeft(const QModelIndex &index);
        void cellMoveRight(const QModelIndex &index);
        void cellMergeUp(const QModelIndex &index);

        // Line operations
        void cellNewLine(const QModelIndex &index);
        void addPrevLine(const QModelIndex &index);
        void addNextLine(const QModelIndex &index);
        void removeLine(const QModelIndex &index);

        // Fermata operations
        void collapseFermata();
        void expandFermata();

        bool fermataState = false;

        int modelMaxCol = 0;

    private:
        QTableView *tableView;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_PHONICMODEL_H
