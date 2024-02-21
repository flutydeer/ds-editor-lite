#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardItemModel>
#include <utility>

#include "PhonicWidget.h"

#include "../Actions/Cell/CellActions.h"
#include "../Actions/Line/LineActions.h"
#include "../Actions/Model/ModelActions.h"

namespace FillLyric {

    PhonicWidget::PhonicWidget(QList<PhonicNote *> phonicNotes, QWidget *parent)
        : g2p_man(G2pMandarin::instance()), g2p_jp(G2pJapanese::instance()),
          m_phonicNotes(std::move(phonicNotes)), QWidget(parent) {

        // 创建模型和视图
        tableView = new QTableView();
        model = new PhonicModel(tableView);
        tableView->setModel(model);

        // 隐藏行头
        tableView->horizontalHeader()->hide();
        QFont font = tableView->font();
        font.setPointSize(12);
        tableView->setFont(font);

        // 打开右键菜单
        tableView->setContextMenuPolicy(Qt::CustomContextMenu);

        // 设置委托
        delegate = new PhonicDelegate(tableView);
        tableView->setItemDelegate(delegate);

        eventFilter = new PhonicEventFilter(tableView, model, this);
        tableView->installEventFilter(eventFilter);

        // 右键菜单
        connect(tableView, &QTableView::customContextMenuRequested, this,
                &PhonicWidget::_on_showContextMenu);

        // PhonicDelegate signals
        connect(delegate, &PhonicDelegate::lyricEdited, this, &PhonicWidget::_on_cellEditClosed);

        // PhonicEventFilter signals
        connect(eventFilter, &PhonicEventFilter::fontSizeChanged, this, [this] { resizeTable(); });
        connect(eventFilter, &PhonicEventFilter::cellClear, this, &PhonicWidget::cellClear);
        connect(eventFilter, &PhonicEventFilter::lineBreak, this, &PhonicWidget::lineBreak);
    }

    PhonicWidget::~PhonicWidget() = default;

    void PhonicWidget::_init(const QList<Phonic> &phonics) {
        QList<QStringList> lyricRes;
        QList<LyricTypeList> labelRes;

        QStringList curLineLyric;
        LyricTypeList curLineLabel;
        for (const auto &phonic : phonics) {
            curLineLyric.append(phonic.lyric);
            curLineLabel.append(phonic.lyricType);
            if (phonic.lineFeed) {
                lyricRes.append(curLineLyric);
                labelRes.append(curLineLabel);
                curLineLyric.clear();
                curLineLabel.clear();
            }
        }
        if (!curLineLyric.isEmpty()) {
            lyricRes.append(curLineLyric);
            labelRes.append(curLineLabel);
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
            QStringList syllables = g2p_man->hanziToPinyin(lyrics, false, false);

            for (int j = 0; j < lyricRes[i].size(); j++) {
                // 设置歌词
                model->setLyric(i, j, lyrics[j]);
                // 设置歌词类型
                model->setLyricType(i, j, labels[j]);
                // 设置注音
                model->setSyllable(i, j, syllables[j]);

                auto candidateSyllables = g2p_man->getDefaultPinyin(lyrics[j], false);

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

                if (labels[j] == TextType::Kana) {
                    auto romaji = g2p_jp->kanaToRomaji(lyrics[j]).at(0);
                    model->setSyllable(i, j, romaji);
                    model->setCandidates(i, j, QStringList() << romaji);
                }
            }
        }

        resizeTable();
        model->shrinkModel();
    }

    QList<Phonic> PhonicWidget::updateLyric(QModelIndex index, const QString &text,
                                            const QList<Phonic> &oldPhonics) {
        int col = index.column();

        QList<Phonic> newPhonics;
        for (const auto &phonic : oldPhonics) {
            newPhonics.append(phonic);
        }
        newPhonics[col].lyric = text;

        auto lyricType = CleanLyric::lyricType(text, "-");

        if (lyricType == TextType::Slur) {
            newPhonics[col].lyricType = TextType::Slur;
            newPhonics[col].syllable = text;
            newPhonics[col].candidates = QStringList() << text;
            return newPhonics;
        }

        // 获取当前行所有单元格的DisplayRole
        QStringList lyrics;
        for (const auto &newPhonic : newPhonics) {
            lyrics.append(newPhonic.lyric);
        }

        auto syllables = g2p_man->hanziToPinyin(lyrics, false, false);
        // 设置当前行所有单元格的Syllable
        for (int i = 0; i < oldPhonics.size(); i++) {
            newPhonics[i].syllable = syllables[i];
        }

        // 设置当前行所有单元格的Candidate
        for (int i = 0; i < oldPhonics.size(); i++) {
            newPhonics[i].candidates = g2p_man->getDefaultPinyin(lyrics[i], false);
        }

        // 设置当前行所有单元格的LyricType
        for (int i = 0; i < oldPhonics.size(); i++) {
            lyricType = CleanLyric::lyricType(lyrics[i], "-");
            newPhonics[i].lyricType = lyricType;
            if (lyricType == TextType::Kana) {
                auto romajiList = g2p_jp->kanaToRomaji(lyrics[i]);
                if (!romajiList.isEmpty()) {
                    const auto &romaji = romajiList.at(0);
                    newPhonics[i].syllable = romaji;
                    newPhonics[i].candidates = QStringList() << romaji;
                }
            }
        }
        return newPhonics;
    }

    void PhonicWidget::_on_cellEditClosed(QModelIndex index, const QString &text) {
        QList<Phonic> oldPhonicList;
        // 获取当前单元格所在行列
        int row = index.row();
        int col = index.column();

        if (row < 0 || col < 0) {
            return;
        }

        // 取整行的数据
        for (int i = 0; i < model->columnCount(); i++) {
            oldPhonicList.append(model->takeData(row, i));
        }

        QList<Phonic> newPhonicList = updateLyric(index, text, oldPhonicList);

        auto a = new CellActions();
        a->cellEdit(index, model, oldPhonicList, newPhonicList);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::resizeTable() {
        // 获取当前字体高度
        int fontHeight = tableView->fontMetrics().height();
        // 行高设置为两倍字体高度
        tableView->verticalHeader()->setDefaultSectionSize((int) (fontHeight * rowHeightRatio));

        // 获取当前字体宽度
        int fontWidth = tableView->fontMetrics().xHeight();

        // 获取比当前字体小3号的字体宽度
        int fontWidthSmall = QFontMetrics(QFont(tableView->font().family(),
                                                tableView->font().pointSize() - fontSizeDiff))
                                 .xHeight();

        int maxFontWidth =
            std::max(fontWidth * (maxLyricLength + 1), fontWidthSmall * (maxSyllableLength + 1));
        auto columnWidth =
            std::max((int) (fontHeight * rowHeightRatio * cellAspectRatio), maxFontWidth);

        // 列宽设置为maxSyllableLength倍字体宽度
        tableView->horizontalHeader()->setDefaultSectionSize(columnWidth);
    }

    void PhonicWidget::_on_btnToggleFermata_clicked() {
        auto a = new ModelActions;
        a->toggleFermata(model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::setFontSizeDiff(int diff) {
        fontSizeDiff = diff;
        resizeTable();
    }

    void PhonicWidget::setAspectRatio(double ratio) {
        cellAspectRatio = ratio;
        resizeTable();
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
                menu->addAction("清空单元格", [this, selected]() { cellClear(selected); });
            } else {
                // 添加候选音节
                _on_changeSyllable(index, menu);
                // 自定义音节
                _on_changePhonetic(index, menu);
                menu->addSeparator();

                // 移动单元格
                menu->addAction("插入空白单元格", [this, index]() { insertCell(index); });
                // 清空单元格
                menu->addAction("清空单元格", [this, selected]() { cellClear(selected); });
                // 向左归并单元格
                if (col > 0) {
                    menu->addAction("向左归并单元格", [this, index]() { cellMergeLeft(index); });
                }
                menu->addAction("删除当前单元格", [this, index]() { deleteCell(index); });
                menu->addSeparator();

                // 换行
                if (model->cellLyricType(row, col) != TextType::Slur)
                    menu->addAction("换行", [this, index]() { lineBreak(index); });
                // 合并到上一行
                if (row > 0 && col == 0)
                    menu->addAction("合并到上一行", [this, index]() { lineMergeUp(index); });

                // 添加上一行
                menu->addAction("向上插入空白行", [this, index]() { addPrevLine(index); });
                // 添加下一行
                menu->addAction("向下插入空白行", [this, index]() { addNextLine(index); });
                // 删除当前行
                menu->addAction("删除当前行", [this, index]() { removeLine(index); });
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
                cellChangePhonic(index, syllable);
            }
        });
    }

    void PhonicWidget::_on_changeSyllable(const QModelIndex &index, QMenu *menu) {
        // 获取当前单元格的候选音节
        QStringList candidateSyllables = index.data(PhonicRole::Candidate).toStringList();

        // 把候选音节添加到菜单，点击后设置为当前单元格的UserRoles
        for (const auto &syllable : candidateSyllables) {
            if (candidateSyllables.size() > 1) {
                menu->addAction(syllable,
                                [this, index, syllable]() { cellChangePhonic(index, syllable); });
            }
        }
    }

    QList<Phonic> PhonicWidget::exportPhonics() {
        QList<Phonic> res;
        for (int i = 0; i < model->rowCount(); i++) {
            int curCol = model->currentLyricLength(i);
            for (int j = 0; j < curCol; j++) {
                Phonic phonic = model->takeData(i, j);
                if (j == curCol - 1) {
                    phonic.lineFeed = true;
                }
                res.append(phonic);
            }
        }
        return res;
    }

    void PhonicWidget::cellClear(const QList<QModelIndex> &indexes) {
        auto a = new CellActions();
        a->cellClear(indexes, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::deleteCell(const QModelIndex &index) {
        auto a = new CellActions();
        a->deleteCell(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::insertCell(const QModelIndex &index) {
        auto a = new CellActions();
        a->insertCell(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::cellMergeLeft(const QModelIndex &index) {
        auto a = new CellActions();
        a->cellMergeLeft(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::cellChangePhonic(const QModelIndex &index, const QString &syllableRevised) {
        auto a = new CellActions();
        a->cellChangePhonic(index, model, syllableRevised);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    // Line Operations
    void PhonicWidget::lineBreak(QModelIndex index) {
        auto a = new LineActions();
        a->lineBreak(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::addPrevLine(QModelIndex index) {
        auto a = new LineActions();
        a->addPrevLine(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::addNextLine(QModelIndex index) {
        auto a = new LineActions();
        a->addNextLine(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::removeLine(QModelIndex index) {
        auto a = new LineActions();
        a->removeLine(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::lineMergeUp(QModelIndex index) {
        auto a = new LineActions();
        a->lineMergeUp(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

}
