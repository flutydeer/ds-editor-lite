#include "PhonicModel.h"

namespace FillLyric {
    // Gui functions
    void PhonicModel::repaintItem(QModelIndex index, const QString &text) {
        Q_EMIT dynamic_cast<PhonicDelegate *>(this->tableView->itemDelegate())
            ->lyricEdited(index, text);
    }

    int PhonicModel::shrinkModel() {
        // 从右到左遍历所有行，找到最长的一行，赋值给modelMaxCol
        int maxCol = 0;
        for (int i = 0; i < this->rowCount(); i++) {
            maxCol = std::max(maxCol, currentLyricLength(i));
        }
        modelMaxCol = maxCol;
        this->setColumnCount(modelMaxCol);
        return maxCol;
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
        QList<int> roles;
        roles.append(Qt::DisplayRole);
        for (int i = PhonicRole::Syllable; i <= PhonicRole::LineFeed; i++) {
            roles.append(i);
        }
        return roles;
    }

    QString PhonicModel::cellLyric(const int row, int col) {
        if (abs(col) >= this->columnCount()) {
            return {};
        } else if (col < 0) {
            col = this->columnCount() + col;
        }
        return this->data(this->index(row, col), Qt::DisplayRole).toString();
    }

    bool PhonicModel::setLyric(const int row, const int col, QString lyric) {
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

    QStringList PhonicModel::cellCandidates(const int row, const int col) {
        return this->data(this->index(row, col), PhonicRole::Candidate).toStringList();
    }

    bool PhonicModel::setCandidates(int row, int col, const QStringList &candidate) {
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

    bool PhonicModel::cellLineFeed(const int row, const int col) {
        return this->data(this->index(row, col), PhonicRole::LineFeed).toBool();
    }

    bool PhonicModel::setLineFeed(const int row, const int col, const bool lineFeed) {
        this->setData(this->index(row, col), lineFeed, PhonicRole::LineFeed);
        return true;
    }

    // Cell operations
    void PhonicModel::putData(const int row, const int col, const Phonic &phonic) {
        // 将phonic的内容放到row行col列
        setLyric(row, col, phonic.lyric);
        setSyllable(row, col, phonic.syllable);
        setCandidates(row, col, phonic.candidates);
        setSyllableRevised(row, col, phonic.SyllableRevised);
        setLyricType(row, col, phonic.type);
        setFermata(row, col, phonic.fermata);
        setLineFeed(row, col, phonic.lineFeed);
    }

    Phonic PhonicModel::takeData(const int row, const int col) {
        Phonic phonic;
        // 根据span的包含的角色，将row行col列的数据取出
        phonic.lyric = cellLyric(row, col);
        phonic.syllable = cellSyllable(row, col);
        phonic.candidates = cellCandidates(row, col);
        phonic.SyllableRevised = cellSyllableRevised(row, col);
        phonic.type = (LyricType) cellLyricType(row, col);
        phonic.fermata = cellFermata(row, col);
        phonic.lineFeed = cellLineFeed(row, col);
        return phonic;
    }

    void PhonicModel::clearData(const int row, const int col) {
        auto roles = allRoles();
        // 根据span的包含的角色，将row行col列的数据清空
        for (int role : roles) {
            this->setData(this->index(row, col), QVariant(), role);
        }
    }

    void PhonicModel::moveData(const int row, const int col, const int tarRow, const int tarCol) {
        auto roles = allRoles();
        if (row == tarRow && col == tarCol) {
            return;
        }
        // 源数据超出范围，目标数据在范围内，清空目标数据
        if (col >= this->columnCount() && this->index(tarRow, tarCol).isValid()) {
            clearData(tarRow, tarCol);
        }
        // 源数据在范围内，目标数据超出范围，扩展模型
        if (tarRow >= this->rowCount()) {
            for (int i = this->rowCount(); i <= tarRow; i++) {
                this->insertRow(i);
            }
        }
        if (tarCol >= this->columnCount()) {
            expandModel(tarCol - this->columnCount() + 1);
        }
        // 根据span的包含的角色，将row行col列的数据移动到tarRow行tarCol列
        for (int role : roles) {
            this->setData(this->index(tarRow, tarCol), this->data(this->index(row, col), role),
                          role);
            this->setData(this->index(row, col), QVariant(), role);
        }
    }

    void PhonicModel::cellPut(const QModelIndex &index, const Phonic &phonic) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 将phonic的内容放到当前单元格
        putData(row, col, phonic);
    }

    Phonic PhonicModel::cellTake(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 获取当前单元格的内容
        return takeData(row, col);
    }

    void PhonicModel::cellClear(const QModelIndex &index) {
        // 清空当前单元格
        clearData(index.row(), index.column());
    }

    void PhonicModel::cellMove(const QModelIndex &source, const QModelIndex &target) {
        // 获取当前单元格坐标
        int row = source.row();
        int col = source.column();
        // 将source的内容移动到target
        moveData(row, col, target.row(), target.column());
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
        repaintItem(this->index(row, col - 1), mergedLyric);
    }

    void PhonicModel::cellMoveLeft(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 将当前的单元格的内容移动到左边的单元格，右边单元格的内容依次向左移动
        for (int i = col; 0 < i && i < this->columnCount(); i++) {
            moveData(row, i, row, i - 1);
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
            moveData(row, i - 1, row, i);
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
            moveData(row, i, row - 1, lastCol + i + 1);
        }

        // 删除当前行
        this->removeRow(row);
    }

    // Line operations
    void PhonicModel::lineBreak(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 在当前行下方新建一行
        this->insertRow(row + 1);
        // 将当前行col列及之后的内容移动到新行，从新行的第一列开始
        for (int i = col; i < this->columnCount(); i++) {
            moveData(row, i, row + 1, i - col);
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
                        setCandidates(row, pos + j + 1, QStringList() << fermataList[j]);
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