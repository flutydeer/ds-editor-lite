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

        cfgLayout->addStretch(1);
        cfgLayout->addWidget(btnInsertText);
        cfgLayout->addWidget(btnToTable);
        cfgLayout->addWidget(btnToText);
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

        // tableView的itemDelegate的closeEditor信号触发_on_cellChanged
        connect(tableView->itemDelegate(), &QAbstractItemDelegate::closeEditor, this,
                &PhonicWidget::_on_cellEditClosed);
    }

    PhonicWidget::~PhonicWidget() = default;

    void PhonicWidget::_on_cellEditClosed() {
        _on_cellChanged(tableView->currentIndex());
    }

    int PhonicWidget::currentLyricLength(const int row) {
        for (int i = model->columnCount() - 1; i >= 0; i--) {
            if (!model->data(model->index(row, i), Qt::DisplayRole).toString().isEmpty()) {
                return i;
            }
        }
        return 0;
    }


    void PhonicWidget::_on_btnToTable_clicked() {
        // 获取文本框的内容
        QString text = textEdit->toPlainText();
        auto cleanRes = CleanLyric::cleanLyric(text);
        // 清空表格
        model->clear();
        // 设置行列数
        // res中最长的一行的长度为列数
        for (auto &line : cleanRes) {
            if (line.size() > modelMaxCol) {
                modelMaxCol = (int) line.size();
            }
        }
        model->setColumnCount(modelMaxCol);
        model->setRowCount((int) cleanRes.size());

        // 记录syllable长度
        int maxSyllableLength = 0;

        // 设置表格内容
        for (int i = 0; i < cleanRes.size(); i++) {
            auto lyrics = cleanRes[i].join("");
            QStringList syllables = g2p_man.hanziToPinyin(lyrics, false, false);
            // maxSyllableLength为syllables中最长的string的长度
            for (const auto &syllable : syllables) {
                if (syllable.length() > maxSyllableLength) {
                    maxSyllableLength = (int) syllable.length();
                }
            }

            for (int j = 0; j < cleanRes[i].size(); j++) {
                // 设置歌词
                model->setData(model->index(i, j), lyrics[j], Qt::DisplayRole);
                // 设置注音
                model->setData(model->index(i, j), syllables[j], Qt::UserRole);

                auto candidateSyllables = g2p_man.getDefaultPinyin(lyrics[j], false);

                if (candidateSyllables.size() > 1) {
                    // 设置候选音节
                    model->setData(model->index(i, j), candidateSyllables, Qt::UserRole + 1);
                }
            }

            // 获取当前字体高度
            int fontHeight = tableView->fontMetrics().height();
            // 行高设置为两倍字体高度
            tableView->verticalHeader()->setDefaultSectionSize(fontHeight * 2 - 2);

            // 获取当前字体宽度
            int fontWidth = tableView->fontMetrics().xHeight();
            // 列宽设置为maxSyllableLength倍字体宽度
            tableView->horizontalHeader()->setDefaultSectionSize(fontWidth * maxSyllableLength + 2);
        }
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
            menu->addSeparator();

            // 移动单元格
            menu->addAction("插入空白单元格", [this, index]() { _on_cellMoveRight(index); });
            // 清空单元格
            menu->addAction("清空单元格", [this, index]() { _on_cellClear(index); });
            // 向左移动单元格
            if (col > 0)
                menu->addAction("向左移动单元格", [this, index]() { _on_cellMoveLeft(index); });
            menu->addSeparator();

            // 换行
            menu->addAction("换行", [this, index]() { _on_cellNewLine(index); });
            // 合并到上一行
            if (row > 0 && col == 0)
                menu->addAction("合并到上一行", [this, index]() { _on_cellMergeUp(index); });
            // 显示菜单
            menu->exec(QCursor::pos());
        }
    }

    void PhonicWidget::_on_cellClear(const QModelIndex &index) {
        // 清空当前单元格
        model->setData(index, "", Qt::DisplayRole);
        model->setData(index, "", Qt::UserRole);
        model->setData(index, QStringList(), Qt::UserRole + 1);
    }

    void PhonicWidget::_on_cellMoveLeft(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 将当前的单元格的内容移动到左边的单元格，右边单元格的内容依次向左移动
        for (int i = col; 0 < i && i <= model->columnCount() - 1; i++) {
            model->setData(model->index(row, i - 1),
                           model->data(model->index(row, i), Qt::DisplayRole), Qt::DisplayRole);
            model->setData(model->index(row, i - 1),
                           model->data(model->index(row, i), Qt::UserRole), Qt::UserRole);
            model->setData(model->index(row, i - 1),
                           model->data(model->index(row, i), Qt::UserRole + 1), Qt::UserRole + 1);
        }
        // 清空最右侧的单元格
        model->setData(model->index(row, model->columnCount() - 1), "", Qt::DisplayRole);
        model->setData(model->index(row, model->columnCount() - 1), "", Qt::UserRole);
        model->setData(model->index(row, model->columnCount() - 1), QStringList(),
                       Qt::UserRole + 1);
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
            model->setData(model->index(row, i),
                           model->data(model->index(row, i - 1), Qt::DisplayRole), Qt::DisplayRole);
            model->setData(model->index(row, i),
                           model->data(model->index(row, i - 1), Qt::UserRole), Qt::UserRole);
            model->setData(model->index(row, i),
                           model->data(model->index(row, i - 1), Qt::UserRole + 1),
                           Qt::UserRole + 1);
        }
        // 清空当前单元格
        model->setData(index, "", Qt::DisplayRole);
        model->setData(index, "", Qt::UserRole);
        model->setData(index, QStringList(), Qt::UserRole + 1);
    }

    void PhonicWidget::_on_cellNewLine(const QModelIndex &index) {
        // 获取当前单元格坐标
        int row = index.row();
        int col = index.column();
        // 在当前行下方新建一行
        model->insertRow(row + 1);
        // 将当前行col列及之后的内容移动到新行，从新行的第一列开始
        for (int i = col; i < model->columnCount(); i++) {
            model->setData(model->index(row + 1, i - col),
                           model->data(model->index(row, i), Qt::DisplayRole), Qt::DisplayRole);
            model->setData(model->index(row + 1, i - col),
                           model->data(model->index(row, i), Qt::UserRole), Qt::UserRole);
            model->setData(model->index(row + 1, i - col),
                           model->data(model->index(row, i), Qt::UserRole + 1), Qt::UserRole + 1);
        }

        // 清空当前行col列及之后的内容
        for (int i = col; i < model->columnCount(); i++) {
            model->setData(model->index(row, i), "", Qt::DisplayRole);
            model->setData(model->index(row, i), "", Qt::UserRole);
            model->setData(model->index(row, i), QStringList(), Qt::UserRole + 1);
        }

        // 从右到左遍历所有行，找到最长的一行，赋值给modelMaxCol
        int maxCol = 0;
        for (int i = 0; i < model->rowCount(); i++) {
            maxCol = std::max(maxCol, currentLyricLength(i));
        }
        modelMaxCol = maxCol;
        model->setColumnCount(modelMaxCol + 1);
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
            model->setData(model->index(row - 1, lastCol + 1 + i),
                           model->data(model->index(row, i), Qt::DisplayRole), Qt::DisplayRole);
            model->setData(model->index(row - 1, lastCol + 1 + i),
                           model->data(model->index(row, i), Qt::UserRole), Qt::UserRole);
            model->setData(model->index(row - 1, lastCol + 1 + i),
                           model->data(model->index(row, i), Qt::UserRole + 1), Qt::UserRole + 1);
        }

        // 删除当前行
        model->removeRow(row);
    }

    void PhonicWidget::_on_changeSyllable(const QModelIndex &index, QMenu *menu) {
        // 获取当前单元格的候选音节
        QStringList candidateSyllables = index.data(Qt::UserRole + 1).toStringList();

        // 把候选音节添加到菜单，点击后设置为当前单元格的UserRoles
        for (const auto &syllable : candidateSyllables) {
            if (candidateSyllables.size() > 1) {
                menu->addAction(syllable, [this, index, syllable]() {
                    model->setData(index, syllable, Qt::UserRole);
                });
            }
        }
    }

    void PhonicWidget::_on_btnInsertText_clicked() {
        // 测试文本
        QString text =
            "蝉声陪伴着行云流浪\n回忆开始后安静遥望远方\n荒草覆没的古井枯塘\n匀散一缕过往\n";
        textEdit->setText(text);
    }

    void PhonicWidget::_on_btnExport_clicked() {
        // 获取DIsplayRole的内容到lyric，获取UserRole的内容到syllable
        QStringList lyricRes;
        QStringList syllableRes;
        for (int i = 0; i < model->rowCount(); i++) {
            QStringList lyricLine;
            QStringList syllableLine;
            for (int j = 0; j < model->columnCount(); j++) {
                auto lyric = model->data(model->index(i, j), Qt::DisplayRole).toString();
                auto syllable = model->data(model->index(i, j), Qt::UserRole).toString();
                if (!lyric.isEmpty()) {
                    lyricLine.append(lyric);
                    syllableLine.append(syllable);
                }
            }
            lyricRes.append(lyricLine.join(""));
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
            model->setData(model->index(row, i), syllables[i], Qt::UserRole);
        }

        // 设置当前行所有单元格的UserRole+1的内容
        for (int i = 0; i < model->columnCount(); i++) {
            model->setData(model->index(row, i), g2p_man.getDefaultPinyin(lyrics[i], false),
                           Qt::UserRole + 1);
        }
    }
}
