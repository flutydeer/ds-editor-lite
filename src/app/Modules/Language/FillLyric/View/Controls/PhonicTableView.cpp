#include "PhonicTableView.h"
#include <QMessageBox>
#include <QInputDialog>

#include <QHeaderView>

#include "../../History/ModelHistory.h"

#include "../../Actions/Cell/CellActions.h"
#include "../../Actions/WrapCell/WrapCellActions.h"
#include "../../Actions/Line/LineActions.h"
#include "../../Actions/WrapLine/WrapLineActions.h"
#include "../../Actions/Model/ModelActions.h"

namespace FillLyric {

    PhonicTableView::PhonicTableView(QWidget *parent)
        : langMgr(LangMgr::ILanguageManager::instance()), QTableView(parent) {
        QFont font = this->font();
        font.setPointSize(12);
        this->setFont(font);
        this->setContextMenuPolicy(Qt::CustomContextMenu);

        model = new PhonicModel(this);
        QTableView::setModel(model);

        this->horizontalHeader()->hide();
        this->verticalHeader()->hide();

        delegate = new PhonicDelegate(this);
        this->setItemDelegate(delegate);

        eventFilter = new PhonicEventFilter(this, model, this);
        this->installEventFilter(eventFilter);

        connect(this, &PhonicTableView::sizeChanged, this, [this] { tableAutoWrap(); });

        // context menu
        connect(this, &QTableView::customContextMenuRequested, this,
                &PhonicTableView::_on_showContextMenu);

        // PhonicDelegate signals
        connect(delegate, &PhonicDelegate::lyricEdited, this, &PhonicTableView::_on_cellEditClosed);
        connect(delegate, &PhonicDelegate::setToolTip, this, &PhonicTableView::_on_setToolTip);
        connect(delegate, &PhonicDelegate::clearToolTip, this, &PhonicTableView::_on_clearToolTip);

        // PhonicEventFilter signals
        connect(eventFilter, &PhonicEventFilter::fontSizeChanged, this, [this] { resizeTable(); });
        connect(eventFilter, &PhonicEventFilter::cellClear, this, &PhonicTableView::cellClear);
        connect(eventFilter, &PhonicEventFilter::lineBreak, this, &PhonicTableView::lineBreak);
    }

    PhonicTableView::~PhonicTableView() = default;

    void PhonicTableView::wheelEvent(QWheelEvent *event) {
        this->wheelEventSignal();
        QTableView::wheelEvent(event);
    }


    void PhonicTableView::resizeEvent(QResizeEvent *event) {
        this->sizeChanged();
        QTableView::resizeEvent(event);
    }

    void PhonicTableView::_init(const QList<Phonic> &phonics) {
        QList<QStringList> lyricRes;
        QList<QStringList> langRes;
        QList<QStringList> cateRes;

        QStringList curLineLyric;
        QStringList curLineLang;
        QStringList curLineCate;
        for (const auto &phonic : phonics) {
            if (phonic.lineFeed) {
                lyricRes.append(curLineLyric);
                langRes.append(curLineLang);
                cateRes.append(curLineCate);
                curLineLyric.clear();
                curLineLang.clear();
                curLineCate.clear();
                continue;
            }
            curLineLyric.append(phonic.lyric);
            curLineLang.append(phonic.language);
            curLineCate.append(phonic.category);
        }
        if (!curLineLyric.isEmpty()) {
            lyricRes.append(curLineLyric);
            langRes.append(curLineLang);
            cateRes.append(curLineCate);
        }

        model->clear();
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

        // Fill the model with the lyrics
        for (int i = 0; i < lyricRes.size(); i++) {
            const auto lyrics = lyricRes[i];
            const auto langs = langRes[i];
            const auto categories = cateRes[i];

            QList<LangNote *> langNote;
            for (int j = 0; j < lyrics.size(); j++) {
                langNote.append(new LangNote(lyrics[j], langs[j], categories[j]));
            }
            langMgr->convert(langNote);

            for (int j = 0; j < lyricRes[i].size(); j++) {
                const auto syllable = langNote[j]->syllable;
                const auto g2pError = langNote[j]->g2pError;
                const auto candidateSyllables = langNote[j]->candidates;

                model->setLyric(i, j, lyrics[j]);
                model->setLanguage(i, j, langs[j]);
                model->setCategory(i, j, categories[j]);
                model->setSyllable(i, j, syllable);
                model->setG2pError(i, j, g2pError);
                if (candidateSyllables.size() > 1) {
                    model->setData(model->index(i, j), candidateSyllables, Candidate);
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

    QList<Phonic> PhonicTableView::updateLyric(const QModelIndex &index, const QString &text,
                                               const QList<Phonic> &oldPhonics) const {
        const int col = index.column();

        QList<Phonic> newPhonics;
        for (const auto &phonic : oldPhonics) {
            newPhonics.append(phonic);
        }
        newPhonics[col].lyric = text;

        if (text == "-") {
            newPhonics[col].language = "Slur";
            newPhonics[col].category = "Slur";
            newPhonics[col].syllable = text;
            newPhonics[col].g2pError = false;
            newPhonics[col].candidates = QStringList() << text;
            return newPhonics;
        }

        QList<LangNote *> langNote;
        for (const auto &newPhonic : newPhonics) {
            langNote.append(new LangNote(newPhonic.lyric, newPhonic.language));
        }

        langMgr->correct(langNote);
        langMgr->convert(langNote);

        for (int i = 0; i < oldPhonics.size(); i++) {
            newPhonics[i].syllable = langNote[i]->syllable;
            newPhonics[i].category = langNote[i]->category;
            newPhonics[i].g2pError = langNote[i]->g2pError;
            newPhonics[i].candidates = langNote[i]->candidates;
            newPhonics[i].language = langNote[i]->language;
        }
        return newPhonics;
    }

    void PhonicTableView::_on_cellEditClosed(const QModelIndex &index, const QString &text) const {
        QList<Phonic> oldPhonicList;

        const int row = index.row();
        const int col = index.column();

        if (row < 0 || col < 0) {
            return;
        }

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
            const QList<LangNote *> langNote = {new LangNote(text, "Unknown")};
            langMgr->correct(langNote);
            langMgr->convert(langNote);

            newPhonic.lyric = text;
            newPhonic.syllable = langNote[0]->syllable;
            newPhonic.category = langNote[0]->category;
            newPhonic.g2pError = langNote[0]->g2pError;
            newPhonic.candidates = langNote[0]->candidates;
            newPhonic.language = langNote[0]->language;

            const auto a = new WrapCellActions();
            a->warpCellEdit(index, model, newPhonic);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicTableView::_on_setToolTip(const QModelIndex &index) const {
        model->setData(index, model->cellLyric(index.row(), index.column()), Qt::ToolTipRole);
    }

    void PhonicTableView::_on_clearToolTip(const QModelIndex &index) const {
        model->setData(index, QVariant(), Qt::ToolTipRole);
    }

    void PhonicTableView::tableAutoWrap(const bool &switchState) const {
        if (!autoWrap) {
            return;
        }

        const int tableWidth = this->width();
        const int colWidth = this->columnWidth(0);

        const bool headerVisible = this->verticalHeader()->isVisible();
        const int headerWidth = headerVisible ? this->verticalHeader()->width() : 0;

        const bool scrollBarVisible = this->verticalScrollBar()->isVisible();
        const int scrollBarWidth = scrollBarVisible ? this->verticalScrollBar()->width() : 0;

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

    void PhonicTableView::resizeTable() const {
        const int fontHeight = this->fontMetrics().height();
        this->verticalHeader()->setDefaultSectionSize(
            static_cast<int>(fontHeight * rowHeightRatio));

        const int fontXHeight = this->fontMetrics().xHeight();
        this->horizontalHeader()->setDefaultSectionSize(
            static_cast<int>(fontXHeight * colWidthRatio));

        if (autoWrap)
            tableAutoWrap();
    }

    void PhonicTableView::_on_btnToggleFermata_clicked() const {
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

    void PhonicTableView::setAutoWrap(const bool &wrap) {
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

    void PhonicTableView::setColWidthRatio(double ratio) {
        colWidthRatio = ratio;
        resizeTable();
    }

    void PhonicTableView::setRowHeightRatio(const double &ratio) {
        rowHeightRatio = ratio;
        resizeTable();
    }

    void PhonicTableView::_on_showContextMenu(const QPoint &pos) {
        const QModelIndex &index = this->indexAt(pos);
        auto selected = this->selectionModel()->selectedIndexes();

        if (index.isValid()) {
            const int row = index.row();
            const int col = index.column();

            auto *menu = new QMenu(this);

            if (selected.count() > 1) {
                menu->addAction(tr("Clear Cell"), [this, selected]() { cellClear(selected); });
            } else {
                _on_changeSyllable(index, menu);
                _on_changePhonetic(index, menu);
                menu->addSeparator();

                menu->addAction(tr("Insert New Cell"), [this, index]() { insertCell(index); });
                menu->addAction(tr("Clear Cell"), [this, selected]() { cellClear(selected); });
                if (col > 0 && !autoWrap) {
                    menu->addAction(tr("Merge Cell To Left"),
                                    [this, index]() { cellMergeLeft(index); });
                }
                menu->addAction(tr("Remove Cell"), [this, index]() { deleteCell(index); });
                menu->addSeparator();

                if (model->cellLanguage(row, col) != "Slur" && model->cellLyric(row, col) != "-" &&
                    !autoWrap)
                    menu->addAction(tr("LineBreak"), [this, index]() { lineBreak(index); });
                if (row > 0 && col == 0 && !autoWrap)
                    menu->addAction(tr("Merge To Up Line"),
                                    [this, index]() { lineMergeUp(index); });

                menu->addAction(tr("Add Prev Line"), [this, index]() { addPrevLine(index); });
                menu->addAction(tr("Add Next Line"), [this, index]() { addNextLine(index); });
                menu->addAction(tr("Remove Cur Line"), [this, index]() { removeLine(index); });
            }

            menu->exec(QCursor::pos());
        }
    }

    void PhonicTableView::_on_changePhonetic(const QModelIndex &index, QMenu *menu) {
        auto *inputAction = new QAction(tr("Custom Syllables"), this);
        menu->addAction(inputAction);
        connect(inputAction, &QAction::triggered, this, [this, index]() {
            bool ok;
            const QString syllable =
                QInputDialog::getText(this, tr("Custom Syllables"), tr("Please input syllables"),
                                      QLineEdit::Normal, "", &ok);
            if (ok && !syllable.isEmpty()) {
                cellChangePhonic(index, syllable);
            }
        });
    }

    void PhonicTableView::_on_changeSyllable(const QModelIndex &index, QMenu *menu) const {
        QStringList candidateSyllables = index.data(PhonicRole::Candidate).toStringList();

        for (const auto &syllable : candidateSyllables) {
            if (candidateSyllables.size() > 1) {
                menu->addAction(syllable,
                                [this, index, syllable]() { cellChangePhonic(index, syllable); });
            }
        }
    }

    void PhonicTableView::cellClear(const QList<QModelIndex> &indexes) const {
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

    void PhonicTableView::deleteCell(const QModelIndex &index) const {
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

    void PhonicTableView::insertCell(const QModelIndex &index) const {
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

    void PhonicTableView::cellMergeLeft(const QModelIndex &index) const {
        const auto a = new CellActions();
        a->cellMergeLeft(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

    void PhonicTableView::cellChangePhonic(const QModelIndex &index,
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
    void PhonicTableView::lineBreak(const QModelIndex &index) const {
        if (!autoWrap) {
            const auto a = new LineActions();
            a->lineBreak(index, model);
            a->execute();
            ModelHistory::instance()->record(a);
        }
    }

    void PhonicTableView::addPrevLine(const QModelIndex &index) const {
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

    void PhonicTableView::addNextLine(const QModelIndex &index) const {
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

    void PhonicTableView::removeLine(const QModelIndex &index) const {
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

    void PhonicTableView::lineMergeUp(const QModelIndex &index) const {
        const auto a = new LineActions();
        a->lineMergeUp(index, model);
        a->execute();
        ModelHistory::instance()->record(a);
    }

} // FillLyric