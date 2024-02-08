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
        model = new PhonicModel();
        tableView = new QTableView();

        // 隐藏行头
        tableView->horizontalHeader()->hide();
        tableView->setModel(model);

        // 打开右键菜单
        tableView->setContextMenuPolicy(Qt::CustomContextMenu);

        // 设置委托
        tableView->setItemDelegate(new PhonicDelegate(tableView));

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
        auto index = tableView->currentIndex();
        // 获取当前单元格所在行列
        int row = index.row();
        int col = index.column();

        if (row < 0 || col < 0) {
            return;
        }

        model->setData(index, CleanLyric::lyricType(model->cellLyric(row, col), "-"),
                       PhonicRole::LyricType);

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
            if (line.size() > model->modelMaxCol) {
                model->modelMaxCol = (int) line.size();
            }
        }
        model->setColumnCount(model->modelMaxCol);
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

        model->shrinkModel();
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
        if (model->fermataState) {
            model->expandFermata();
        } else {
            model->collapseFermata();
        }
        model->fermataState = !model->fermataState;
        model->shrinkModel();
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
            menu->addAction("插入空白单元格", [this, index]() { model->cellMoveRight(index); });
            // 清空单元格
            menu->addAction("清空单元格", [this, index]() { model->cellClear(index); });
            // 向左归并单元格
            if (col > 0)
                menu->addAction("向左归并单元格", [this, index]() { model->cellMergeLeft(index); });
            // 向左移动单元格
            if (col > 0)
                menu->addAction("向左移动单元格", [this, index]() { model->cellMoveLeft(index); });
            // 向右移动单元格
            menu->addAction("向右移动单元格", [this, index]() { model->cellMoveRight(index); });
            menu->addSeparator();

            // 换行
            if (model->cellLyricType(row, col) != LyricType::Fermata)
                menu->addAction("换行", [this, index]() { model->cellNewLine(index); });
            // 合并到上一行
            if (row > 0 && col == 0)
                menu->addAction("合并到上一行", [this, index]() { model->cellMergeUp(index); });

            // 添加上一行
            menu->addAction("向上插入空白行", [this, index]() { model->addPrevLine(index); });
            // 添加下一行
            menu->addAction("向下插入空白行", [this, index]() { model->addNextLine(index); });
            // 删除当前行
            menu->addAction("删除当前行", [this, index]() { model->removeLine(index); });

            // 显示菜单
            menu->exec(QCursor::pos());
        }
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

    void PhonicWidget::setLyrics(const QString &lyrics) {
        // 设置文本框内容
        textEdit->setText(lyrics);
    }

    void PhonicWidget::repaintTable() {
        // 重绘表格
        emit tableView->itemDelegate()->closeEditor(nullptr, QAbstractItemDelegate::NoHint);
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
