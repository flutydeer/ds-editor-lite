#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardItemModel>

#include "Utils/LrcTools/LrcDecoder.h"
#include "PhonicWidget.h"

namespace FillLyric {

    PhonicWidget::PhonicWidget(QObject *parent)
        : g2p_man(new IKg2p::Mandarin()), g2p_jp(new IKg2p::JpG2p()) {
        // 创建一个多行文本框
        textEdit = new PhonicTextEdit();
        textEdit->setPlaceholderText("请输入歌词");

        // 创建模型和视图
        tableView = new QTableView();
        model = new PhonicModel(tableView);

        // 隐藏行头
        tableView->horizontalHeader()->hide();
        tableView->setModel(model);

        // 打开右键菜单
        tableView->setContextMenuPolicy(Qt::CustomContextMenu);

        // 设置委托
        tableView->setItemDelegate(new PhonicDelegate(tableView));

        eventFilter = new PhonicEventFilter(tableView, model, this);
        tableView->installEventFilter(eventFilter);

        // 创建布局
        topLayout = new QHBoxLayout();
        textCountLabel = new QLabel("");
        noteCountLabel = new QLabel("");
        topLayout->addWidget(textCountLabel);
        topLayout->addStretch(1);
        topLayout->addWidget(noteCountLabel);

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

        mainLayout = new QVBoxLayout(this);
        mainLayout->addLayout(topLayout);
        mainLayout->addLayout(tableLayout);

        connect(btnToTable, &QAbstractButton::clicked, this, &PhonicWidget::_on_btnToTable_clicked);
        connect(tableView, &QTableView::customContextMenuRequested, this,
                &PhonicWidget::_on_showContextMenu);

        connect(btnInsertText, &QAbstractButton::clicked, this,
                &PhonicWidget::_on_btnInsertText_clicked);
        connect(btnToText, &QAbstractButton::clicked, this, &PhonicWidget::_on_btnToText_clicked);
        connect(btnToggleFermata, &QAbstractButton::clicked, this,
                &PhonicWidget::_on_btnToggleFermata_clicked);
        connect(btnImportLrc, &QAbstractButton::clicked, this,
                &PhonicWidget::_on_btnImportLrc_clicked);

        // tableView的itemDelegate的closeEditor信号触发_on_cellChanged
        connect(tableView->itemDelegate(), &QAbstractItemDelegate::closeEditor, this,
                &PhonicWidget::_on_cellEditClosed);

        // fontsize
        connect(eventFilter, &PhonicEventFilter::fontSizeChanged, this, [this] { resizeTable(); });

        // count
        connect(textEdit, &PhonicTextEdit::textChanged, this, &PhonicWidget::_on_textEditChanged);
        connect(model, &PhonicModel::dataChanged, this, &PhonicWidget::_on_modelDataChanged);
    }

    PhonicWidget::~PhonicWidget() = default;

    void PhonicWidget::_init(QList<QList<QString>> lyricRes) {
        QList<QList<CleanLyric::LyricType>> labelRes;
        for (auto &line : lyricRes) {
            QList<CleanLyric::LyricType> labelLine;
            for (auto &lyric : line) {
                labelLine.append(CleanLyric::lyricType(lyric, "-"));
            }
            labelRes.append(labelLine);
        }

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

        // 设置表格内容
        for (int i = 0; i < lyricRes.size(); i++) {
            auto lyrics = lyricRes[i];
            auto labels = labelRes[i];
            QStringList syllables = g2p_man.hanziToPinyin(lyrics, false, false);

            for (int j = 0; j < lyricRes[i].size(); j++) {
                // 设置歌词
                model->setLyric(i, j, lyrics[j]);
                // 设置歌词类型
                model->setLyricType(i, j, labels[j]);
                // 设置注音
                model->setSyllable(i, j, syllables[j]);

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

                if (labels[j] == LyricType::Kana) {
                    auto romaji = g2p_jp.kanaToRomaji(lyrics[j]).at(0);
                    model->setSyllable(i, j, romaji);
                    model->setCandidate(i, j, QStringList() << romaji);
                }
            }
        }

        resizeTable();
        model->shrinkModel();
    }

    void PhonicWidget::_on_cellEditClosed() {
        auto index = tableView->currentIndex();
        // 获取当前单元格所在行列
        int row = index.row();
        int col = index.column();

        if (row < 0 || col < 0) {
            return;
        }

        auto lyricType = CleanLyric::lyricType(model->cellLyric(row, col), "-");

        if (lyricType == LyricType::Fermata) {
            auto lyric = model->cellLyric(row, col);
            model->setLyricType(row, col, LyricType::Fermata);
            model->setSyllable(row, col, lyric);
            model->setCandidate(row, col, QStringList() << lyric);
            return;
        }

        // 获取当前行所有单元格的DisplayRole
        QStringList lyrics;
        for (int i = 0; i < model->columnCount(); i++) {
            lyrics.append(model->cellLyric(row, i));
        }

        auto syllables = g2p_man.hanziToPinyin(lyrics, false, false);
        // 设置当前行所有单元格的Syllable
        for (int i = 0; i < model->columnCount(); i++) {
            model->setSyllable(row, i, syllables[i]);
        }

        // 设置当前行所有单元格的Candidate
        for (int i = 0; i < model->columnCount(); i++) {
            model->setCandidate(row, i, g2p_man.getDefaultPinyin(lyrics[i], false));
        }

        // 设置当前行所有单元格的LyricType
        for (int i = 0; i < model->columnCount(); i++) {
            lyricType = CleanLyric::lyricType(lyrics[i], "-");
            model->setLyricType(row, i, lyricType);
            if (lyricType == LyricType::Kana) {
                auto romaji = g2p_jp.kanaToRomaji(lyrics[i]).at(0);
                model->setSyllable(row, i, romaji);
                model->setCandidate(row, i, QStringList() << romaji);
            }
        }
    }

    void PhonicWidget::_on_textEditChanged() {
        // 获取文本框的内容
        QString text = textEdit->toPlainText();
        // 获取歌词
        auto res = CleanLyric::cleanLyric(text).first;
        int lyricCount = 0;
        for (auto &line : res) {
            lyricCount += (int) line.size();
        }
        textCountLabel->setText(QString("字符数: %1").arg(lyricCount));
    }

    void PhonicWidget::_on_modelDataChanged() {
        int lyricCount = 0;
        for (int i = 0; i < model->rowCount(); i++) {
            lyricCount += model->currentLyricLength(i);
        }
        noteCountLabel->setText(QString::number(lyricCount) + "/" + QString::number(notesCount));
    }

    void PhonicWidget::_on_btnToTable_clicked() {
        // 获取文本框的内容
        QString text = textEdit->toPlainText();
        _init(CleanLyric::cleanLyric(text).first);
    }

    void PhonicWidget::resizeTable() {
        // 获取当前字体高度
        int fontHeight = tableView->fontMetrics().height();
        // 行高设置为两倍字体高度
        tableView->verticalHeader()->setDefaultSectionSize(fontHeight * 2);

        // 获取当前字体宽度
        int fontWidth = tableView->fontMetrics().xHeight();
        // 列宽设置为maxSyllableLength倍字体宽度
        tableView->horizontalHeader()->setDefaultSectionSize(
            fontWidth * std::max(maxLyricLength, maxSyllableLength) + fontWidth * 4);
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
    }

    void PhonicWidget::_on_showContextMenu(const QPoint &pos) {
        // 获取点击位置的索引
        QModelIndex index = tableView->indexAt(pos);
        // 获取当前选中的单元格
        auto selected = tableView->selectionModel()->selectedIndexes();

        // 验证点击位置是否在表格内
        if (index.isValid()) {
            // 获取当前行列数
            int row = index.row();
            int col = index.column();

            // 创建菜单
            auto *menu = new QMenu(tableView);

            if (selected.count() > 1) {
                // 清空单元格
                menu->addAction("清空单元格", [this, selected]() { model->cellClear(selected); });
            } else {
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
                if (col > 0) {
                    menu->addAction("向左归并单元格",
                                    [this, index]() { model->cellMergeLeft(index); });
                    menu->addAction("向左移动单元格",
                                    [this, index]() { model->cellMoveLeft(index); });
                }
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
            }

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

    QList<Phonic> PhonicWidget::exportPhonics() {
        QList<Phonic> res;
        for (int i = 0; i < model->rowCount(); i++) {
            int curCol = model->currentLyricLength(i);
            for (int j = 0; j < curCol; j++) {
                Phonic phonic;
                phonic.lyric = model->cellLyric(i, j);
                phonic.syllable = model->cellSyllable(i, j);
                phonic.SyllableRevised = model->cellSyllableRevised(i, j);
                phonic.type = CleanLyric::LyricType(model->cellLyricType(i, j));
                if (j == curCol - 1) {
                    phonic.lineFeed = true;
                }
                res.append(phonic);
            }
        }
        return res;
    }

    void PhonicWidget::setLyrics(QList<QList<QString>> &lyrics) {
        notesCount = 0;
        QString text;
        for (auto &line : lyrics) {
            notesCount += (int) line.size();
            text.append(line.join(""));
            text.append("\n");
        }
        // 设置文本框内容
        textEdit->setText(text);
        // 初始化表格
        _init(lyrics);
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
