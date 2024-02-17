#include "PhonicModel.h"

namespace FillLyric {
    // Gui functions
    void PhonicModel::repaintView() {
        emit this->tableView->itemDelegate()->closeEditor(nullptr, QAbstractItemDelegate::NoHint);
    }

    void PhonicModel::shrinkModel() {
        // 从右到左遍历所有行，找到最长的一行，赋值给modelMaxCol
        int maxCol = 0;
        for (int i = 0; i < this->rowCount(); i++) {
            maxCol = std::max(maxCol, currentLyricLength(i));
        }
        modelMaxCol = maxCol;
        this->setColumnCount(modelMaxCol);
    }

    void PhonicModel::expandModel(const int col) {
        int maxCol = this->columnCount();
        this->setColumnCount(maxCol + col);
    }

    // Basic functions
    int PhonicModel::currentLyricLength(const int row) {
        for (int i = this->columnCount() - 1; i >= 0; i--) {
            if (!cellLyric(row, i).isEmpty()) {
                return i + 1;
            }
        }
        return 0;
    }

    // RoleData functions
    QList<int> PhonicModel::allRoles() {
        return {Qt::DisplayRole,       PhonicRole::Syllable,
                PhonicRole::Candidate, PhonicRole::SyllableRevised,
                PhonicRole::LyricType, PhonicRole::FermataAddition};
    }

    QString PhonicModel::cellLyric(const int row, int col) {
        if (abs(col) >= this->columnCount()) {
            return {};
        } else if (col < 0) {
            col = this->columnCount() + col;
        }
        return this->data(this->index(row, col), Qt::DisplayRole).toString();
    }

    bool PhonicModel::setLyric(const int row, const int col, QString &lyric) {
        this->setData(this->index(row, col), lyric, Qt::DisplayRole);
        return true;
    }

    QString PhonicModel::cellSyllable(const int row, const int col) {
        return this->data(this->index(row, col), PhonicRole::Syllable).toString();
    }

    bool PhonicModel::setSyllable(const int row, const int col, const QString &syllable) {
        this->setData(this->index(row, col), syllable, PhonicRole::Syllable);
        return true;
    }

    QString PhonicModel::cellSyllableRevised(const int row, const int col) {
        return this->data(this->index(row, col), PhonicRole::SyllableRevised).toString();
    }

    bool PhonicModel::setSyllableRevised(const int row, const int col,
                                         const QString &syllableRevised) {
        this->setData(this->index(row, col), syllableRevised, PhonicRole::SyllableRevised);
        return true;
    }

    QStringList PhonicModel::cellCandidate(const int row, const int col) {
        return this->data(this->index(row, col), PhonicRole::Candidate).toStringList();
    }

    bool PhonicModel::setCandidate(const int row, const int col, const QStringList &candidate) {
        this->setData(this->index(row, col), candidate, PhonicRole::Candidate);
        return true;
    }

    int PhonicModel::cellLyricType(const int row, const int col) {
        return this->data(this->index(row, col), PhonicRole::LyricType).toInt();
    }

    bool PhonicModel::setLyricType(const int row, const int col, LyricType type) {
        this->setData(this->index(row, col), type, PhonicRole::LyricType);
        return true;
    }

    QStringList PhonicModel::cellFermata(const int row, const int col) {
        return this->data(this->index(row, col), PhonicRole::FermataAddition).toStringList();
    }

    bool PhonicModel::setFermata(const int row, const int col, const QStringList &fermata) {
        this->setData(this->index(row, col), fermata, PhonicRole::FermataAddition);
        return true;
    }

    // Cell operations
    void PhonicModel::clearData(const int row, const int col, const QList<int> &roles) {
        // 根据span的包含的角色，将row行col列的数据清空
        for (int role : roles) {
            this->setData(this->index(row, col), QVariant(), role);
        }
    }

    void PhonicModel::moveData(const int row, const int col, const int tarRow, const int tarCol,
                               const QList<int> &roles) {
        // 根据span的包含的角色，将row行col列的数据移动到tarRow行tarCol列
        for (int role : roles) {
            this->setData(this->index(tarRow, tarCol), this->data(this->index(row, col), role),
                          role);
            this->setData(this->index(row, col), QVariant(), role);
        }
    }

    void PhonicModel::cellClear(const QModelIndex &index) {
        // 清空当前单元格
        clearData(index.row(), index.column(), allRoles());
    }

    void PhonicModel::cellMergeLeft(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 获取左侧单元格的内容和当前单元格的DisplayRole的内容，合并到左侧单元格
        QString mergedLyric = cellLyric(row, col - 1) + cellLyric(row, col);
        setLyric(row, col - 1, mergedLyric);

        // 获取右侧单元格的index
        cellMoveLeft(this->index(row, col + 1));
        repaintView();
    }

    void PhonicModel::cellMoveLeft(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 将当前的单元格的内容移动到左边的单元格，右边单元格的内容依次向左移动
        for (int i = col; 0 < i && i < this->columnCount(); i++) {
            moveData(row, i, row, i - 1, allRoles());
        }
        if (col == this->columnCount()) {
            clearData(row, this->columnCount(), allRoles());
        }
    }

    void PhonicModel::cellMoveRight(const QModelIndex &index) {
        //  获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 将对应的单元格的内容移动到右边的单元格，右边单元格的内容依次向右移动，超出范围的部分向右新建单元格
        QString lastLyric = cellLyric(row, -1);
        if (!lastLyric.isEmpty()) {
            expandModel(1);
        }

        // 向右移动
        for (int i = this->columnCount() - 1; i > col; i--) {
            moveData(row, i - 1, row, i, allRoles());
        }
    }

    void PhonicModel::cellMergeUp(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        // 从右向左遍历上一行，找到第一个DisplayRole不为空的单元格
        int lastCol = currentLyricLength(row - 1);

        // 从右向左遍历当前行，找到第一个DisplayRole不为空的单元格
        int currentCol = currentLyricLength(row);

        // 根据lastCol和currentCol的和，扩展模型的列数
        modelMaxCol = std::max(modelMaxCol, lastCol + currentCol);
        if (modelMaxCol + 1 > this->columnCount()) {
            this->setColumnCount(modelMaxCol);
        }

        // 将当前行的内容移动到上一行，从上一行的lastCol+1列开始放当前行的第一列
        for (int i = 0; i < currentCol; i++) {
            moveData(row, i, row - 1, lastCol + i + 1, allRoles());
        }

        // 删除当前行
        this->removeRow(row);
    }

    // Multi-cell operations
    void PhonicModel::cellClear(const QModelIndexList &indexList) {
        // 清空indexList中的所有单元格
        for (const auto &index : indexList) {
            clearData(index.row(), index.column(), allRoles());
        }
    }

    // Line operations
    void PhonicModel::cellNewLine(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 在当前行下方新建一行
        this->insertRow(row + 1);
        // 将当前行col列及之后的内容移动到新行，从新行的第一列开始
        for (int i = col; i < this->columnCount(); i++) {
            moveData(row, i, row + 1, i - col, allRoles());
        }
        shrinkModel();
    }

    void PhonicModel::addPrevLine(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        // 在当前行上方新建一行
        this->insertRow(row);
    }

    void PhonicModel::addNextLine(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        // 在当前行下方新建一行
        this->insertRow(row + 1);
    }

    void PhonicModel::removeLine(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        // 删除当前行
        this->removeRow(row);

        shrinkModel();
    }

    // Fermata operations
    void PhonicModel::collapseFermata() {
        // 遍历模型每行
        for (int row = 0; row < this->rowCount(); row++) {
            int pos = 1;
            while (pos < currentLyricLength(row)) {
                // 获取当前单元格的内容
                auto currentType = cellLyricType(row, pos);
                if (currentType == LyricType::Fermata) {
                    int start = pos;
                    while (pos < this->columnCount() &&
                           cellLyricType(row, pos) == LyricType::Fermata) {
                        pos++;
                    }

                    // 把pos-1的单元格的FermataRole设为折叠的FermataList
                    QStringList fermataList;
                    for (int j = start; j < pos; j++) {
                        fermataList.append(cellLyric(row, j));
                    }
                    setFermata(row, start - 1, fermataList);

                    // 右侧数据左移、覆盖延音符号
                    for (int k = 0; k < fermataList.size(); k++) {
                        cellMoveLeft(this->index(row, pos - k));
                    }
                    pos = 1;
                } else {
                    pos++;
                }
            }
        }
    }

    void PhonicModel::expandFermata() {
        // 遍历模型每行
        for (int row = 0; row < this->rowCount(); row++) {
            int pos = 0;
            // 遍历每行的每个单元格
            while (pos < this->columnCount()) {
                // 获取当前单元格的FermataRole的内容
                auto fermataList = cellFermata(row, pos);

                if (!fermataList.isEmpty()) {
                    // 在右侧插入空白单元格
                    if (pos + fermataList.size() > this->columnCount() - 1) {
                        expandModel(pos + (int) fermataList.size() - (this->columnCount() - 1));
                    } else {
                        for (int j = 0; j < fermataList.size(); j++) {
                            cellMoveRight(this->index(row, pos + 1));
                        }
                    }
                    // 将pos右侧的fermataList.size()个单元格的内容设置为fermataList[j]
                    for (int j = 0; j < fermataList.size(); j++) {
                        setLyric(row, pos + j + 1, fermataList[j]);
                        setSyllable(row, pos + j + 1, fermataList[j]);
                        setCandidate(row, pos + j + 1, QStringList() << fermataList[j]);
                        setLyricType(row, pos + j + 1, LyricType::Fermata);
                    }
                    // 清空pos的FermataRole
                    setFermata(row, pos, QStringList());
                    pos = 0;
                }
                pos++;
            }
        }
    }

} // FillLyric