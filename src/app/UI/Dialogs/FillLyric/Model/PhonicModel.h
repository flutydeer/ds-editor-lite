#ifndef DS_EDITOR_LITE_PHONICMODEL_H
#define DS_EDITOR_LITE_PHONICMODEL_H

#include <QApplication>

#include <QMenu>
#include <QWidget>
#include <QScrollBar>
#include <QVBoxLayout>

#include "g2pglobal.h"

#include "PhonicCommon.h"
#include "../View/Controls/PhonicDelegate.h"

namespace FillLyric {

    class PhonicModel final : public QStandardItemModel {
        Q_OBJECT
    public:
        explicit PhonicModel(QTableView *tableView, QObject *parent = nullptr)
            : QStandardItemModel(parent), m_tableView(tableView) {
        }

        // Gui functions
        void repaintItem(const QModelIndex &index, const QString &text) const;
        int shrinkModel();
        void expandModel(int col);

        // Basic functions
        [[nodiscard]] int currentLyricLength(int row) const;
        void refreshTable();
        void shrinkPhonicList();

        // RoleData functions
        static QList<int> allRoles();

        [[nodiscard]] QString cellLyric(int row, int col) const;
        bool setLyric(int row, int col, const QString &lyric);
        [[nodiscard]] QString cellSyllable(int row, int col) const;
        bool setSyllable(int row, int col, const QString &syllable);
        [[nodiscard]] QString cellSyllableRevised(int row, int col) const;
        bool setSyllableRevised(int row, int col, const QString &syllableRevised);
        [[nodiscard]] QStringList cellCandidates(int row, int col) const;
        bool setCandidates(const int &row, const int &col, const QStringList &candidate);
        [[nodiscard]] int cellLyricType(int row, int col) const;
        bool setLyricType(int row, int col, const TextType &type);
        [[nodiscard]] QStringList cellFermata(int row, int col) const;
        bool setFermata(int row, int col, const QList<QString> &fermata);
        [[nodiscard]] bool cellLineFeed(int row, int col) const;
        bool setLineFeed(int row, int col, bool lineFeed);
        [[nodiscard]] QString cellToolTip(const int &row, const int &col) const;

        // Cell operations
        void putData(int row, int col, const Phonic &phonic);
        [[nodiscard]] Phonic takeData(int row, int col) const;
        void clearData(int row, int col);
        void moveData(int row, int col, int tarRow, int tarCol);

        void insertWarpCell(const int &index, const Phonic &phonic);
        void editWarpCell(const int &index, const Phonic &phonic);
        void deleteWarpCell(const int &index);

        void putCell(const QModelIndex &index, const Phonic &phonic);
        [[nodiscard]] Phonic takeCell(const QModelIndex &index) const;
        void clearCell(const QModelIndex &index);
        void moveCell(const QModelIndex &source, const QModelIndex &target);

        [[nodiscard]] QString cellToolTip(const QModelIndex &index) const;

        void cellMoveLeft(const QModelIndex &index);
        void cellMoveRight(const QModelIndex &index);

        // Fermata operations
        void collapseFermata();
        void expandFermata();

        bool fermataState = false;

        QList<Phonic> m_phonics;

    private:
        QTableView *m_tableView;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_PHONICMODEL_H
