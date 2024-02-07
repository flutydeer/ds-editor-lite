#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardItemModel>

#include "Utils/LrcTools/LrcDecoder.h"
#include "PhonicWidget.h"

namespace FillLyric {

    PhonicWidget::PhonicWidget(QObject *parent) : g2p_man(new IKg2p::Mandarin()) {
        // 创建一个多行文本框
        textEdit = new QTextEdit();
        textEdit->setPlaceholderText("请输入歌词");

        // 创建模型和视图
        model = new QStandardItemModel();
        tableView = new QTableView();

        // 隐藏行头
        tableView->horizontalHeader()->hide();
        tableView->setModel(model);

        // 打开右键菜单
        tableView->setContextMenuPolicy(Qt::CustomContextMenu);

        // 设置委托
        tableView->setItemDelegate(new SyllableDelegate(tableView));

        cfgLayout = new QVBoxLayout();
        btnInsertText = new QPushButton("插入测试文本");
        btnToTable = new QPushButton(">>");
        btnToText = new QPushButton("<<");
        btnImportLrc = new QPushButton("导入lrc");
        btnToggleFermata = new QPushButton("收放延音符");

        cfgLayout->addStretch(1);
        cfgLayout->addWidget(btnInsertText);
        cfgLayout->addWidget(btnToTable);
        cfgLayout->addWidget(btnToText);
        cfgLayout->addWidget(btnToggleFermata);
        cfgLayout->addWidget(btnImportLrc);
        cfgLayout->addStretch(1);

        // 文本框在左边，表格在右边，中间放一个">>"按钮
        tableLayout = new QHBoxLayout();
        tableLayout->addWidget(textEdit);
        tableLayout->addLayout(cfgLayout);
        tableLayout->addWidget(tableView);

        // 底部按钮
        bottomLayout = new QHBoxLayout();
        btnExport = new QPushButton("导出");
        btnCancel = new QPushButton("取消");
        bottomLayout->addStretch(1);
        bottomLayout->addWidget(btnExport);
        bottomLayout->addWidget(btnCancel);

        mainLayout = new QVBoxLayout(this);
        mainLayout->addLayout(tableLayout);
        mainLayout->addLayout(bottomLayout);

        connect(btnToTable, &QAbstractButton::clicked, this, &PhonicWidget::_on_btnToTable_clicked);
        connect(tableView, &QTableView::customContextMenuRequested, this,
                &PhonicWidget::_on_showContextMenu);

        connect(btnInsertText, &QAbstractButton::clicked, this,
                &PhonicWidget::_on_btnInsertText_clicked);
        connect(btnToText, &QAbstractButton::clicked, this, &PhonicWidget::_on_btnToText_clicked);
        connect(btnExport, &QAbstractButton::clicked, this, &PhonicWidget::_on_btnExport_clicked);
        connect(btnToggleFermata, &QAbstractButton::clicked, this,
                &PhonicWidget::_on_btnToggleFermata_clicked);
        connect(btnImportLrc, &QAbstractButton::clicked, this,
                &PhonicWidget::_on_btnImportLrc_clicked);

        // tableView的itemDelegate的closeEditor信号触发_on_cellChanged
        connect(tableView->itemDelegate(), &QAbstractItemDelegate::closeEditor, this,
                &PhonicWidget::_on_cellEditClosed);
    }

    PhonicWidget::~PhonicWidget() = default;

    void PhonicWidget::_on_cellEditClosed() {
        _on_cellChanged(tableView->currentIndex());
    }

    int PhonicWidget::cellLyricType(const int row, const int col) {
        return model->data(model->index(row, col), PhonicRole::LyricType).toInt();
    }

    QString PhonicWidget::cellLyric(const int row, const int col) {
        return model->data(model->index(row, col), Qt::DisplayRole).toString();
    }

    int PhonicWidget::currentLyricLength(const int row) {
        for (int i = model->columnCount() - 1; i >= 0; i--) {
            if (!model->data(model->index(row, i), Qt::DisplayRole).toString().isEmpty()) {
                return i;
            }
        }
        return 0;
    }

    void PhonicWidget::collapseFermata() {
        // 遍历模型每行
        for (int i = 0; i < model->rowCount(); i++) {
            int pos = 1;
            while (pos < model->columnCount()) {
                // 获取当前单元格的内容
                auto currentType = cellLyricType(i, pos);
                if (currentType == LyricType::Fermata) {
                    int start = pos;
                    while (pos < model->columnCount() &&
                           cellLyricType(i, pos) == LyricType::Fermata) {
                        pos++;
                    }

                    // 把pos-1的单元格的FermataRole设为折叠的FermataList
                    QStringList fermataList;
                    for (int j = start; j < pos; j++) {
                        fermataList.append(cellLyric(i, j));
                    }
                    model->setData(model->index(i, start - 1), fermataList, PhonicRole::Fermata);

                    // 右侧数据左移、覆盖延音符号
                    for (int k = 0; k < fermataList.size(); k++) {
                        _on_cellMoveLeft(model->index(i, pos - k));
                    }
                    pos = 1;
                } else {
                    pos++;
                }
            }
        }
    }

    void PhonicWidget::expandFermata() {
        // 遍历模型每行
        for (int i = 0; i < model->rowCount(); i++) {
            int pos = 0;
            // 遍历每行的每个单元格
            while (pos < model->columnCount()) {
                // 获取当前单元格的FermataRole的内容
                auto fermataList =
                    model->data(model->index(i, pos), PhonicRole::Fermata).toStringList();

                if (!fermataList.isEmpty()) {
                    // 在右侧插入空白单元格
                    if (pos + fermataList.size() + 1 > model->columnCount()) {
                        model->setColumnCount(model->columnCount() + fermataList.size());
                    } else {
                        for (int j = 0; j < fermataList.size(); j++) {
                            _on_cellMoveRight(model->index(i, pos + 1));
                        }
                    }
                    // 将pos右侧的fermataList.size()个单元格的内容设置为fermataList[j]
                    for (int j = 0; j < fermataList.size(); j++) {
                        setFermata(i, pos + j + 1, fermataList[j]);
                    }
                    // 清空pos的FermataRole
                    model->setData(model->index(i, pos), QVariant(), PhonicRole::Fermata);
                    pos = 0;
                }
                pos++;
            }
        }
    }

    void PhonicWidget::setFermata(const int row, const int col, QString &fermata) {
        model->setData(model->index(row, col), fermata, Qt::DisplayRole);
        model->setData(model->index(row, col), fermata, PhonicRole::Syllable);
        model->setData(model->index(row, col), QStringList(fermata), PhonicRole::Candidate);
        model->setData(model->index(row, col), LyricType::Fermata, PhonicRole::LyricType);
    }

    void PhonicWidget::clearData(const int row, const int col, const QList<int> &roles) {
        // 根据span的包含的角色，将row行col列的数据清空
        for (int role : roles) {
            model->setData(model->index(row, col), QVariant(), role);
        }
    }

    void PhonicWidget::moveData(const int row, const int col, const int tarRow, const int tarCol,
                                const QList<int> &roles) {
        // 根据span的包含的角色，将row行col列的数据移动到tarRow行tarCol列
        for (int role : roles) {
            model->setData(model->index(tarRow, tarCol), model->data(model->index(row, col), role),
                           role);
            model->setData(model->index(row, col), QVariant(), role);
        }
    }

    void PhonicWidget::_on_btnToTable_clicked() {
        // 获取文本框的内容
        QString text = textEdit->toPlainText();
        auto cleanRes = CleanLyric::cleanLyric(text);
        auto lyricRes = cleanRes.first;
        auto labelRes = cleanRes.second;

        // 清空表格
        model->clear();
        // 设置行列数
        // res中最长的一行的长度为列数
        for (auto &line : lyricRes) {
            if (line.size() > modelMaxCol) {
                modelMaxCol = (int) line.size();
            }
        }
        model->setColumnCount(modelMaxCol);
        model->setRowCount((int) lyricRes.size());

        int maxLyricLength = 0;
        // 记录syllable长度
        int maxSyllableLength = 0;

        // 设置表格内容
        for (int i = 0; i < lyricRes.size(); i++) {
            auto lyrics = lyricRes[i];
            auto labels = labelRes[i];
            QStringList syllables = g2p_man.hanziToPinyin(lyrics, false, false);

            for (int j = 0; j < lyricRes[i].size(); j++) {
                // 设置歌词
                model->setData(model->index(i, j), lyrics[j], Qt::DisplayRole);
                // 设置歌词类型
                model->setData(model->index(i, j), labels[j], PhonicRole::LyricType);
                // 设置注音
                model->setData(model->index(i, j), syllables[j], PhonicRole::Syllable);

                auto candidateSyllables = g2p_man.getDefaultPinyin(lyrics[j], false);

                // 候选音节中最长的音节长度
                for (const auto &syllable : candidateSyllables) {
                    maxSyllableLength =
                        std::max(maxSyllableLength, static_cast<int>(syllable.length()));
                }

                maxLyricLength = std::max(maxLyricLength, static_cast<int>(lyrics[j].length()));

                if (candidateSyllables.size() > 1) {
                    // 设置候选音节
                    model->setData(model->index(i, j), candidateSyllables, PhonicRole::Candidate);
                }
            }
        }

        // 获取当前字体高度
        int fontHeight = tableView->fontMetrics().height();
        // 行高设置为两倍字体高度
        tableView->verticalHeader()->setDefaultSectionSize(fontHeight * 2);

        // 获取当前字体宽度
        int fontWidth = tableView->fontMetrics().xHeight();
        // 列宽设置为maxSyllableLength倍字体宽度
        tableView->horizontalHeader()->setDefaultSectionSize(
            fontWidth * std::max(maxLyricLength, maxSyllableLength) + fontWidth * 4);

        shrinkTable();
    }

    void PhonicWidget::_on_btnToText_clicked() {
        // 获取表格内容
        QStringList res;
        for (int i = 0; i < model->rowCount(); i++) {
            QStringList line;
            for (int j = 0; j < model->columnCount(); j++) {
                auto lyric = model->data(model->index(i, j), Qt::DisplayRole).toString();
                if (!lyric.isEmpty()) {
                    line.append(lyric);
                }
            }
            res.append(line.join(""));
        }
        // 设置文本框内容
        textEdit->setText(res.join("\n"));
    }

    void PhonicWidget::_on_btnToggleFermata_clicked() {
        if (fermataState) {
            expandFermata();
        } else {
            collapseFermata();
        }
        fermataState = !fermataState;
        shrinkTable();
        repaintTable();
    }

    void PhonicWidget::_on_showContextMenu(const QPoint &pos) {
        // 获取点击位置的索引
        QModelIndex index = tableView->indexAt(pos);
        // 验证点击位置是否在表格内
        if (index.isValid()) {
            // 获取当前行列数
            int row = index.row();
            int col = index.column();

            // 创建菜单
            auto *menu = new QMenu(tableView);
            // 添加候选音节
            _on_changeSyllable(index, menu);
            // 自定义音节
            _on_changePhonetic(index, menu);
            menu->addSeparator();

            // 移动单元格
            menu->addAction("插入空白单元格", [this, index]() { _on_cellMoveRight(index); });
            // 清空单元格
            menu->addAction("清空单元格", [this, index]() { _on_cellClear(index); });
            // 向左归并单元格
            if (col > 0)
                menu->addAction("向左归并单元格", [this, index]() { _on_cellMergeLeft(index); });
            // 向左移动单元格
            if (col > 0)
                menu->addAction("向左移动单元格", [this, index]() { _on_cellMoveLeft(index); });
            // 向右移动单元格
            menu->addAction("向右移动单元格", [this, index]() { _on_cellMoveRight(index); });
            menu->addSeparator();

            // 换行
            if (cellLyricType(row, col) != LyricType::Fermata)
                menu->addAction("换行", [this, index]() { _on_cellNewLine(index); });
            // 合并到上一行
            if (row > 0 && col == 0)
                menu->addAction("合并到上一行", [this, index]() { _on_cellMergeUp(index); });

            // 添加上一行
            menu->addAction("向上插入空白行", [this, index]() { _on_addPrevLine(index); });
            // 添加下一行
            menu->addAction("向下插入空白行", [this, index]() { _on_addNextLine(index); });
            // 删除当前行
            menu->addAction("删除当前行", [this, index]() { _on_removeLine(index); });

            // 显示菜单
            menu->exec(QCursor::pos());
        }
    }

    void PhonicWidget::_on_cellClear(const QModelIndex &index) {
        // 清空当前单元格
        clearData(index.row(), index.column(), allRoles());
    }

    void PhonicWidget::_on_cellMergeLeft(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 获取左侧单元格的内容和当前单元格的DisplayRole的内容，合并到左侧单元格
        auto leftData = model->data(model->index(row, col - 1), Qt::DisplayRole).toString();
        auto currentData = model->data(index, Qt::DisplayRole).toString();
        model->setData(model->index(row, col - 1), leftData + currentData, Qt::DisplayRole);

        // 获取右侧单元格的index
        _on_cellMoveLeft(model->index(row, col + 1));
        repaintTable();
    }

    void PhonicWidget::_on_cellMoveLeft(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 将当前的单元格的内容移动到左边的单元格，右边单元格的内容依次向左移动
        for (int i = col; 0 < i && i < model->columnCount(); i++) {
            moveData(row, i, row, i - 1, allRoles());
        }
        if (col == model->columnCount()) {
            clearData(row, model->columnCount(), allRoles());
        }
    }


    void PhonicWidget::_on_cellMoveRight(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 将对应的单元格的内容移动到右边的单元格，右边单元格的内容依次向右移动，超出范围的部分向右新建单元格
        // 获取当前行最右一个单元格的列号
        int maxCol = model->columnCount() - 1;
        // 如果maxCol的DisplayRole不为空，模型的列数加一
        if (!model->data(model->index(row, maxCol), Qt::DisplayRole).toString().isEmpty()) {
            model->setColumnCount(maxCol + 2);
        }
        // 向右移动
        for (int i = model->columnCount() - 1; i > col; i--) {
            moveData(row, i - 1, row, i, allRoles());
        }
    }

    void PhonicWidget::_on_cellNewLine(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 在当前行下方新建一行
        model->insertRow(row + 1);
        // 将当前行col列及之后的内容移动到新行，从新行的第一列开始
        for (int i = col; i < model->columnCount(); i++) {
            moveData(row, i, row + 1, i - col, allRoles());
        }
        shrinkTable();
    }

    void PhonicWidget::_on_cellMergeUp(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        // 从右向左遍历上一行，找到第一个DisplayRole不为空的单元格
        int lastCol = currentLyricLength(row - 1);

        // 从右向左遍历当前行，找到第一个DisplayRole不为空的单元格
        int currentCol = currentLyricLength(row);

        // 根据lastCol和currentCol的和，扩展模型的列数
        modelMaxCol = std::max(modelMaxCol, lastCol + currentCol + 2);
        if (modelMaxCol + 1 > model->columnCount()) {
            model->setColumnCount(modelMaxCol);
        }

        // 将当前行的内容移动到上一行，从上一行的lastCol+1列开始放当前行的第一列
        for (int i = 0; i <= currentCol; i++) {
            moveData(row, i, row - 1, lastCol + i + 1, allRoles());
        }

        // 删除当前行
        model->removeRow(row);
    }

    void PhonicWidget::_on_changePhonetic(const QModelIndex &index, QMenu *menu) {
        auto *inputAction = new QAction("自定义音节", tableView);
        menu->addAction(inputAction);
        connect(inputAction, &QAction::triggered, this, [this, index]() {
            bool ok;
            QString syllable = QInputDialog::getText(tableView, "自定义音节", "请输入音节",
                                                     QLineEdit::Normal, "", &ok);
            if (ok && !syllable.isEmpty()) {
                model->setData(index, syllable, PhonicRole::SyllableRevised);
            }
        });
    }

    void PhonicWidget::_on_changeSyllable(const QModelIndex &index, QMenu *menu) {
        // 获取当前单元格的候选音节
        QStringList candidateSyllables = index.data(PhonicRole::Candidate).toStringList();

        // 把候选音节添加到菜单，点击后设置为当前单元格的UserRoles
        for (const auto &syllable : candidateSyllables) {
            if (candidateSyllables.size() > 1) {
                menu->addAction(syllable, [this, index, syllable]() {
                    model->setData(index, syllable, PhonicRole::SyllableRevised);
                });
            }
        }
    }

    void PhonicWidget::_on_btnInsertText_clicked() {
        // 测试文本
        QString text = "蝉声--陪伴着行云流浪---\n回-忆-开始后安静遥望远方\n荒草覆没的古井--"
                       "枯塘\n匀-散一缕过往\n";
        textEdit->setText(text);
    }

    void PhonicWidget::_on_btnExport_clicked() {
        // 获取DisplayRole的内容到lyric，获取UserRole的内容到syllable
        QStringList lyricRes;
        QStringList syllableRes;
        for (int i = 0; i < model->rowCount(); i++) {
            QStringList lyricLine;
            QStringList syllableLine;
            for (int j = 0; j < model->columnCount(); j++) {
                auto lyric = model->data(model->index(i, j), Qt::DisplayRole).toString();
                auto syllable = model->data(model->index(i, j), PhonicRole::Syllable).toString();
                if (!lyric.isEmpty()) {
                    lyricLine.append(lyric);
                    syllableLine.append(syllable);
                }
            }
            if (!lyricLine.isEmpty())
                lyricRes.append(lyricLine.join(""));
            if (!syllableLine.isEmpty())
                syllableRes.append(syllableLine.join(" "));
        }
        qDebug() << "lyricRes: " << lyricRes;
        qDebug() << "syllableRes: " << syllableRes;
    }

    void PhonicWidget::_on_cellChanged(const QModelIndex &index) {
        // 获取当前单元格所在行列
        int row = index.row();

        // 获取当前行所有单元格的DisplayRole的内容
        QStringList lyrics;
        for (int i = 0; i < model->columnCount(); i++) {
            lyrics += model->data(model->index(row, i), Qt::DisplayRole).toString();
        }

        auto syllables = g2p_man.hanziToPinyin(lyrics, false, false);
        // 设置当前行所有单元格的UserRole的内容
        for (int i = 0; i < model->columnCount(); i++) {
            model->setData(model->index(row, i), syllables[i], PhonicRole::Syllable);
        }

        // 设置当前行所有单元格的UserRole+1的内容
        for (int i = 0; i < model->columnCount(); i++) {
            model->setData(model->index(row, i), g2p_man.getDefaultPinyin(lyrics[i], false),
                           PhonicRole::Candidate);
        }
    }

    void PhonicWidget::_on_addPrevLine(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        // 在当前行上方新建一行
        model->insertRow(row);
    }

    void PhonicWidget::_on_addNextLine(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        // 在当前行下方新建一行
        model->insertRow(row + 1);
    }

    void PhonicWidget::_on_removeLine(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        // 删除当前行
        model->removeRow(row);

        shrinkTable();
    }

    void PhonicWidget::shrinkTable() {
        // 从右到左遍历所有行，找到最长的一行，赋值给modelMaxCol
        int maxCol = 0;
        for (int i = 0; i < model->rowCount(); i++) {
            maxCol = std::max(maxCol, currentLyricLength(i));
        }
        modelMaxCol = maxCol;
        model->setColumnCount(modelMaxCol + 1);
    }

    void PhonicWidget::repaintTable() {
        // 重绘表格
        emit tableView->itemDelegate()->closeEditor(nullptr, QAbstractItemDelegate::NoHint);
    }

    QList<int> PhonicWidget::allRoles() {
        return {Qt::DisplayRole,       PhonicRole::Syllable,
                PhonicRole::Candidate, PhonicRole::SyllableRevised,
                PhonicRole::LyricType, PhonicRole::Fermata};
    }

    QList<int> PhonicWidget::displayRole() {
        return {Qt::DisplayRole};
    }

    void PhonicWidget::_on_btnImportLrc_clicked() {
        // 打开文件对话框
        QString fileName =
            QFileDialog::getOpenFileName(this, "打开歌词文件", "", "歌词文件(*.lrc)");
        if (fileName.isEmpty()) {
            return;
        }
        // 创建LrcDecoder对象
        LrcTools::LrcDecoder decoder;
        // 解析歌词文件
        auto res = decoder.decode(fileName);
        if (!res) {
            // 解析失败
            QMessageBox::warning(this, "错误", "解析lrc文件失败");
            return;
        }
        // 获取歌词文件的元数据
        auto metadata = decoder.dumpMetadata();
        qDebug() << "metadata: " << metadata;

        // 获取歌词文件的歌词
        auto lyrics = decoder.dumpLyrics();
        // 设置文本框内容
        textEdit->setText(lyrics.join("\n"));
    }
}
