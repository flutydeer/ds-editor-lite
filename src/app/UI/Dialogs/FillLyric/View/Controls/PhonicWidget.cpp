#include <QMessageBox>
#include <QInputDialog>

#include <QHeaderView>

#include "PhonicWidget.h"
#include "../../History/ModelHistory.h"

#include "../../Actions/Cell/CellActions.h"
#include "../../Actions/WrapCell/WrapCellActions.h"
#include "../../Actions/Line/LineActions.h"
#include "../../Actions/WrapLine/WrapLineActions.h"
#include "../../Actions/Model/ModelActions.h"

namespace FillLyric {
    PhonicWidget::PhonicWidget(QWidget *parent)
        : g2pMgr(G2pMgr::IG2pManager::instance()), QWidget(parent) {

        // 创建模型和视图
        tableView = new PhonicTableView();
        model = new PhonicModel(tableView);
        tableView->setModel(model);

        // 隐藏行头
        tableView->horizontalHeader()->hide();

        // 设置委托
        delegate = new PhonicDelegate(tableView);
        tableView->setItemDelegate(delegate);

        eventFilter = new PhonicEventFilter(tableView, model, this);
        tableView->installEventFilter(eventFilter);

        connect(tableView, &PhonicTableView::sizeChanged, this, [this] { tableAutoWrap(); });

        // 右键菜单
        connect(tableView, &QTableView::customContextMenuRequested, this,
                &PhonicWidget::_on_showContextMenu);

        // PhonicDelegate signals
        connect(delegate, &PhonicDelegate::lyricEdited, this, &PhonicWidget::_on_cellEditClosed);
        connect(delegate, &PhonicDelegate::setToolTip, this, &PhonicWidget::_on_setToolTip);
        connect(delegate, &PhonicDelegate::clearToolTip, this, &PhonicWidget::_on_clearToolTip);

        // PhonicEventFilter signals
        connect(eventFilter, &PhonicEventFilter::fontSizeChanged, this, [this] { resizeTable(); });
        connect(eventFilter, &PhonicEventFilter::cellClear, this, &PhonicWidget::cellClear);
        connect(eventFilter, &PhonicEventFilter::lineBreak, this, &PhonicWidget::lineBreak);
    }

    PhonicWidget::~PhonicWidget() = default;

    void PhonicWidget::_init(const QList<Phonic> &phonics) {
        QList<QStringList> lyricRes;
        QList<LyricTypeList> labelRes;

        const auto g2p_man = g2pMgr->g2p("Mandarin");
        const auto g2p_kana = g2pMgr->g2p("Kana");

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
        int maxCol = 0;
        for (auto &line : lyricRes) {
            if (line.size() > maxCol) {
                maxCol = static_cast<int>(line.size());
            }
        }

        int times = 3;
        while (model->columnCount() != maxCol && maxCol > 0 && times > 0) {
            model->setColumnCount(maxCol);
            times--;
        }

        model->setRowCount(static_cast<int>(lyricRes.size()));

        // 设置表格内容
        for (int i = 0; i < lyricRes.size(); i++) {
            auto lyrics = lyricRes[i];
            auto labels = labelRes[i];
            auto g2pRes = g2p_man->convert(lyrics);

            for (int j = 0; j < lyricRes[i].size(); j++) {
                // 设置歌词
                model->setLyric(i, j, lyrics[j]);
                // 设置歌词类型
                model->setLyricType(i, j, labels[j]);
                // 设置注音
                model->setSyllable(i, j, g2pRes[j].pronunciation.original);

                const auto candidateSyllables = g2pRes[j].candidates;

                if (candidateSyllables.size() > 1) {
                    // 设置候选音节
                    model->setData(model->index(i, j), candidateSyllables, Candidate);
                }

                if (labels[j] == TextType::Kana) {
                    auto romaji = g2p_kana->convert(lyrics[j]).pronunciation.original;
                    model->setSyllable(i, j, romaji);
                    model->setCandidates(i, j, QStringList() << romaji);
                }
            }
        }

        if (autoWrap) {
            model->m_phonics.clear();
            for (int i = 0; i < model->rowCount(); i++) {
                for (int j = 0; j < model->currentLyricLength(i); j++) {
                    model->m_phonics.append(model->takeData(i, j));
                }
            }
        }

        model->shrinkModel();
        resizeTable();
        Q_EMIT this->historyReset();
    }

    QList<Phonic> PhonicWidget::updateLyric(const QModelIndex &index, const QString &text,
                                            const QList<Phonic> &oldPhonics) const {
        const auto g2p_man = g2pMgr->g2p("Mandarin");
        const auto g2p_kana = g2pMgr->g2p("Kana");

        const int col = index.column();

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

        auto g2pRes = g2p_man->convert(lyrics);
        // 设置当前行所有单元格的Syllable
        for (int i = 0; i < oldPhonics.size(); i++) {
            newPhonics[i].syllable = g2pRes[i].pronunciation.original;
        }

        // 设置当前行所有单元格的Candidate
        for (int i = 0; i < oldPhonics.size(); i++) {
            newPhonics[i].candidates = g2pRes[i].candidates;
        }

        // 设置当前行所有单元格的LyricType
        for (int i = 0; i < oldPhonics.size(); i++) {
            lyricType = CleanLyric::lyricType(lyrics[i], "-");
            newPhonics[i].lyricType = lyricType;
            if (lyricType == Kana) {
                const auto kanaRes = g2p_kana->convert(lyrics[i]);
                if (!kanaRes.pronunciation.original.isEmpty()) {
                    newPhonics[i].syllable = kanaRes.pronunciation.original;
                    newPhonics[i].candidates = kanaRes.candidates;
                }
            }
        }
        return newPhonics;
    }

    void PhonicWidget::_on_cellEditClosed(const QModelIndex &index, const QString &text) const {
        const auto g2p_man = g2pMgr->g2p("Mandarin");
        const auto g2p_jp = g2pMgr->g2p("Kana");

        QList<Phonic> oldPhonicList;
        // 获取当前单元格所在行列
        const int row = index.row();
        const int col = index.column();

        if (row < 0 || col < 0) {
            return;
        }

        // 取整行的数据
        for (int i = 0; i < model->columnCount(); i++) {
            oldPhonicList.append(model->takeData(row, i));
        }

        if (!autoWrap) {
            const QList<Phonic> newPhonicList = updateLyric(index, text, oldPhonicList);
            const auto a = new CellActions();
            a->cellEdit(index, model, oldPhonicList, newPhonicList);
            a->execute();
            ModelHistory::instance()->record(a);
        } else {
            Phonic newPhonic = oldPhonicList[col];
            newPhonic.lyric = text;
            newPhonic.lyricType = CleanLyric::lyricType(text, "-");
            if (newPhonic.lyricType == Kana) {
                const auto kanaRes = g2p_jp->convert(text);
                if (!kanaRes.pronunciation.original.isEmpty()) {
                    newPhonic.syllable = kanaRes.pronunciation.original;
                    newPhonic.candidates = kanaRes.candidates;
                }
            } else if (newPhonic.lyricType == Hanzi) {
                const auto g2pRes = g2p_man->convert(text);
                newPhonic.syllable = g2pRes.pronunciation.original;
                newPhonic.candidates = g2pRes.candidates;
            } else {
                newPhonic.syllable = text;
                newPhonic.candidates = QStringList() << text;
            }
            const auto a = new WrapCellActions();
            a->warpCellEdit(index, model, newPhonic);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicWidget::_on_setToolTip(const QModelIndex &index) const {
        model->setData(index, model->cellLyric(index.row(), index.column()), Qt::ToolTipRole);
    }

    void PhonicWidget::_on_clearToolTip(const QModelIndex &index) const {
        model->setData(index, QVariant(), Qt::ToolTipRole);
    }

    void PhonicWidget::tableAutoWrap(const bool &switchState) const {
        if (!autoWrap) {
            return;
        }
        // 计算最大列数
        const int tableWidth = tableView->width();
        const int colWidth = tableView->columnWidth(0);

        const bool headerVisible = tableView->verticalHeader()->isVisible();
        const int headerWidth = headerVisible ? tableView->verticalHeader()->width() : 0;

        const bool scrollBarVisible = tableView->verticalScrollBar()->isVisible();
        const int scrollBarWidth = scrollBarVisible ? tableView->verticalScrollBar()->width() : 0;

        const auto tarCol = static_cast<int>(
            static_cast<double>(tableWidth - headerWidth - scrollBarWidth - 10) / colWidth);
        const auto curCol = model->columnCount();

        const bool tarValid = tarCol != curCol && curCol > 0 && tarCol > 0;
        const bool clearSpace = tarCol == curCol && switchState;
        if (tarValid || clearSpace) {
            model->shrinkPhonicList();
            auto maxRow = static_cast<int>(model->m_phonics.size() / tarCol);
            if (model->m_phonics.size() % tarCol != 0) {
                maxRow++;
            }

            for (int i = static_cast<int>(model->m_phonics.size()); i < maxRow * tarCol; i++) {
                model->m_phonics.append(Phonic());
            }

            model->clear();
            model->setRowCount(maxRow);
            model->setColumnCount(tarCol);
            for (int i = 0; i < model->m_phonics.size(); i++) {
                model->putData(i / tarCol, i % tarCol, model->m_phonics[i]);
            }
        }
    }

    void PhonicWidget::resizeTable() const {
        // 获取当前字体高度
        const int fontHeight = tableView->fontMetrics().height();
        // 行高设置为两倍字体高度
        tableView->verticalHeader()->setDefaultSectionSize(
            static_cast<int>(fontHeight * rowHeightRatio));

        const int fontXHeight = tableView->fontMetrics().xHeight();
        // 列宽设置为maxSyllableLength倍字体宽度
        tableView->horizontalHeader()->setDefaultSectionSize(
            static_cast<int>(fontXHeight * colWidthRatio));

        if (autoWrap)
            tableAutoWrap();
    }

    void PhonicWidget::_on_btnToggleFermata_clicked() const {
        if (!autoWrap) {
            const auto a = new ModelActions();
            a->toggleFermata(model);
            a->execute();
            ModelHistory::instance()->record(a);
        } else {
            const auto a = new ModelActions();
            a->wrapToggleFermata(model);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicWidget::setAutoWrap(const bool &wrap) {
        autoWrap = wrap;
        if (autoWrap) {
            model->m_phonics.clear();
            for (int i = 0; i < model->rowCount(); i++) {
                for (int j = 0; j < model->currentLyricLength(i); j++) {
                    model->m_phonics.append(model->takeData(i, j));
                }
            }
            tableAutoWrap();
        }
    }

    void PhonicWidget::setColWidthRatio(double ratio) {
        colWidthRatio = ratio;
        resizeTable();
    }

    void PhonicWidget::setRowHeightRatio(const double &ratio) {
        rowHeightRatio = ratio;
        resizeTable();
    }

    void PhonicWidget::_on_showContextMenu(const QPoint &pos) {
        // 获取点击位置的索引
        const QModelIndex &index = tableView->indexAt(pos);
        // 获取当前选中的单元格
        auto selected = tableView->selectionModel()->selectedIndexes();

        // 验证点击位置是否在表格内
        if (index.isValid()) {
            // 获取当前行列数
            const int row = index.row();
            const int col = index.column();

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
                if (col > 0 && !autoWrap) {
                    menu->addAction("向左归并单元格", [this, index]() { cellMergeLeft(index); });
                }
                menu->addAction("删除当前单元格", [this, index]() { deleteCell(index); });
                menu->addSeparator();

                // 换行
                if (model->cellLyricType(row, col) != TextType::Slur && !autoWrap)
                    menu->addAction("换行", [this, index]() { lineBreak(index); });
                // 合并到上一行
                if (row > 0 && col == 0 && !autoWrap)
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

    void PhonicWidget::_on_changeSyllable(const QModelIndex &index, QMenu *menu) const {
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

    void PhonicWidget::cellClear(const QList<QModelIndex> &indexes) const {
        if (!autoWrap) {
            const auto a = new CellActions();
            a->cellClear(indexes, model);
            a->execute();
            ModelHistory::instance()->record(a);
        } else {
            const auto a = new WrapCellActions();
            a->warpCellClear(indexes, model);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicWidget::deleteCell(const QModelIndex &index) const {
        if (!autoWrap) {
            const auto a = new CellActions();
            a->deleteCell(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        } else {
            const auto a = new WrapCellActions();
            a->deleteWrapCell(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicWidget::insertCell(const QModelIndex &index) const {
        if (!autoWrap) {
            const auto a = new CellActions();
            a->insertCell(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        } else {
            const auto a = new WrapCellActions();
            a->insertWrapCell(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicWidget::cellMergeLeft(const QModelIndex &index) const {
        const auto a = new CellActions();
        a->cellMergeLeft(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::cellChangePhonic(const QModelIndex &index,
                                        const QString &syllableRevised) const {
        if (!autoWrap) {
            const auto a = new CellActions();
            a->cellChangePhonic(index, model, syllableRevised);
            a->execute();
            ModelHistory::instance()->record(a);
        } else {
            const auto a = new WrapCellActions();
            a->warpCellChangePhonic(index, model, syllableRevised);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    // Line Operations
    void PhonicWidget::lineBreak(const QModelIndex &index) const {
        const auto a = new LineActions();
        a->lineBreak(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicWidget::addPrevLine(const QModelIndex &index) const {
        if (!autoWrap) {
            const auto a = new LineActions();
            a->addPrevLine(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        } else {
            const auto a = new WrapLineActions();
            a->prevWarpLine(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicWidget::addNextLine(const QModelIndex &index) const {
        if (!autoWrap) {
            const auto a = new LineActions();
            a->addNextLine(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        } else {
            const auto a = new WrapLineActions();
            a->nextWarpLine(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicWidget::removeLine(const QModelIndex &index) const {
        if (!autoWrap) {
            const auto a = new LineActions();
            a->removeLine(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        } else {
            const auto a = new WrapLineActions();
            a->removeWarpLine(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicWidget::lineMergeUp(const QModelIndex &index) const {
        const auto a = new LineActions();
        a->lineMergeUp(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }
}
