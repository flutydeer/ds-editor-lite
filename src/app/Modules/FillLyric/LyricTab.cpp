#include "Modules/FillLyric/LyricTab.h"

#include "Modules/FillLyric/Controls/CellList.h"
#include "Modules/FillLyric/Controls/LyricCell.h"

#include <QFileDialog>
#include <QMessageBox>

#include "Modules/FillLyric/Utils/G2pService.h"

#include "Modules/FillLyric/Utils/SplitLyric.h"

namespace FillLyric
{
    LyricTab::LyricTab(const QList<LangNote> &langNotes, const QStringList &priorityG2pIds,
                       QMap<QString, QString> langToG2pId, const LyricTabConfig &config, QWidget *parent)
        : QWidget(parent) {

        for (const auto &g2pId : priorityG2pIds)
            m_priorityG2pIds.push_back(g2pId.toStdString());
        for (auto it = langToG2pId.begin(); it != langToG2pId.end(); ++it)
            m_langToG2pId.insert(it.key().toStdString(), it.value().toStdString());
        for (const auto &langNote : langNotes)
            m_langNotes.append(new LangNote(langNote));

        QList<LangNote> inputNotes;
        for (const auto &note : m_langNotes)
            inputNotes.append(*note);

        const auto g2pResults = G2pService::convert(inputNotes, m_priorityG2pIds, m_langToG2pId);
        for (int i = 0; i < g2pResults.size(); i++) {
            if (m_langNotes[i]->language == QStringLiteral("unknown"))
                m_langNotes[i]->language = g2pResults[i].language;
            if (m_langNotes[i]->g2pId == QStringLiteral("unknown") || m_langNotes[i]->g2pId.isEmpty())
                m_langNotes[i]->g2pId = g2pResults[i].g2pId;
            m_langNotes[i]->syllable = g2pResults[i].syllable;
            m_langNotes[i]->candidates = g2pResults[i].candidates;
        }

        m_lyricBaseWidget = new LyricBaseWidget(config, m_priorityG2pIds, m_langToG2pId);
        m_lyricExtWidget = new LyricExtWidget(&m_notesCount, config, m_priorityG2pIds, m_langToG2pId);

        m_lyricLayout = new QHBoxLayout();
        m_lyricLayout->setContentsMargins(0, 0, 0, 0);
        m_lyricLayout->addWidget(m_lyricBaseWidget, 1);
        m_lyricLayout->addWidget(m_lyricExtWidget, 2);

        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->setContentsMargins(0, 10, 0, 10);
        m_mainLayout->addLayout(m_lyricLayout);

        connect(m_lyricBaseWidget, &LyricBaseWidget::modifyOption, this, &LyricTab::modifyOption);
        connect(m_lyricExtWidget, &LyricExtWidget::modifyOption, this, &LyricTab::modifyOption);

        connect(m_lyricBaseWidget, &LyricBaseWidget::reReadNoteRequested, this, [this] { setLangNotes(false);});
        connect(m_lyricBaseWidget, &LyricBaseWidget::toTableRequested, this, &LyricTab::onBtnToTableClicked);
        connect(m_lyricExtWidget, &LyricExtWidget::insertTextRequested, this, &LyricTab::onBtnInsertTextClicked);

        connect(m_lyricBaseWidget, &LyricBaseWidget::lyricPrevRequested, this,
                [this]
                {
                    m_lyricExtWidget->setVisible(!m_lyricExtWidget->isVisible());
                    m_lyricBaseWidget->setToTableVisible(m_lyricExtWidget->isVisible());
                    m_lyricBaseWidget->setLyricPrevText(
                        m_lyricExtWidget->isVisible() ? tr("Fold Preview") : tr("Lyric Prev"));
                    if (m_lyricExtWidget->isVisible()) {
                        Q_EMIT this->expandWindowRight();
                    } else {
                        Q_EMIT this->shrinkWindowRight(m_lyricBaseWidget->width() + 20);
                    }
                    modifyOption();
                });

        connect(m_lyricExtWidget, &LyricExtWidget::foldLeftRequested, this,
                [this]
                {
                    m_lyricBaseWidget->setVisible(!m_lyricBaseWidget->isVisible());
                    m_lyricExtWidget->setFoldLeftText(
                        m_lyricBaseWidget->isVisible() ? tr("Fold Left") : tr("Expand Left"));
                    modifyOption();
                });

        const bool baseVisible = config.lyricBaseVisible;
        const bool extVisible = config.lyricExtVisible;

        if (!baseVisible) {
            m_lyricBaseWidget->setVisible(baseVisible);
            m_lyricExtWidget->setFoldLeftText(tr("Expand Left"));
        }

        if (!extVisible) {
            m_lyricExtWidget->setVisible(extVisible);
            m_lyricBaseWidget->setToTableVisible(false);
            m_lyricBaseWidget->setLyricPrevText(tr("Lyric Prev"));
        } else {
            m_lyricBaseWidget->setLyricPrevText(tr("Fold Preview"));
        }

        m_lyricBaseWidget->setSkipSlur(config.baseSkipSlur);
        m_exportLanguage = config.exportLanguage;
        connect(m_lyricBaseWidget, &LyricBaseWidget::splitOptionChanged, this, [this] { setLangNotes(false); });
    }

    LyricTab::~LyricTab() {
        qDeleteAll(m_langNotes);
        m_langNotes.clear();
    }

    void LyricTab::setLangNotes(const bool warn) {
        const bool skipSlurRes = m_lyricBaseWidget->skipSlur();

        bool setLangNotes = false;
        if (warn) {
            const QMessageBox::StandardButton res =
                QMessageBox::question(nullptr, tr("Preview Lyric"), tr("Split the lyric into Preview window?"),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (res == QMessageBox::Yes)
                setLangNotes = true;
        }

        if (!warn || setLangNotes) {
            QStringList lyrics;
            QList<LangNote> langNotes;
            for (const auto &langNote : m_langNotes) {
                if (skipSlurRes && (langNote->g2pId == "slur" || langNote->lyric == "-"))
                    continue;
                langNotes.append(*langNote);
                lyrics.append(langNote->lyric);
            }
            m_notesCount = static_cast<int>(langNotes.size());
            m_lyricBaseWidget->setLyricText(lyrics.join(" "));
            m_lyricExtWidget->wrapView()->init({langNotes});
        } else {
            m_lyricBaseWidget->setSkipSlur(!skipSlurRes);
            modifyOption();
        }
    }

    QList<QList<LangNote>> LyricTab::exportLangNotes() const {
        if (m_lyricExtWidget->isVisible()) {
            return this->modelExport();
        }
        auto langNotes = m_lyricBaseWidget->splitLyric(m_lyricBaseWidget->lyricText());

        QList<QList<LangNote>> result;
        for (auto &notes : langNotes) {
            const auto g2pResults = G2pService::convert(notes, m_priorityG2pIds, m_langToG2pId);
            for (int i = 0; i < g2pResults.size(); i++) {
                if (notes[i].language == QStringLiteral("unknown"))
                    notes[i].language = g2pResults[i].language;
                if (notes[i].g2pId == QStringLiteral("unknown") || notes[i].g2pId.isEmpty())
                    notes[i].g2pId = g2pResults[i].g2pId;
                notes[i].syllable = g2pResults[i].syllable;
                notes[i].candidates = g2pResults[i].candidates;
            }
            result.append(notes);
        }
        return result;
    }

    QList<QList<LangNote>> LyricTab::modelExport() const {
        const auto cellLists = m_lyricExtWidget->wrapView()->cellLists();

        QList<QList<LangNote>> noteList;
        for (const auto &cellList : cellLists) {
            QList<LangNote> notes;
            for (const auto &cell : cellList->m_cells) {
                const auto note = cell->note();
                notes.append(*note);
            }
            noteList.append(notes);
        }
        return noteList;
    }

    bool LyricTab::exportSkipSlur() const { return m_lyricBaseWidget->skipSlur(); }

    void LyricTab::onBtnInsertTextClicked() const {
        const QString text = m_lyricBaseWidget->lyricText();
        if (text.isEmpty())
            return;
        m_lyricExtWidget->wrapView()->init(LyricSplitter::splitAuto(text, m_priorityG2pIds));
    }

    void LyricTab::onBtnToTableClicked() const {
        const QString text = m_lyricBaseWidget->lyricText();
        const auto splitRes = m_lyricBaseWidget->splitLyric(text);

        const QMessageBox::StandardButton res =
            QMessageBox::question(nullptr, tr("Preview Lyric"), tr("Split the lyric into Preview window?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (res == QMessageBox::Yes)
            m_lyricExtWidget->wrapView()->init(splitRes);
    }

    void LyricTab::modifyOption() {
        Q_EMIT this->modifyOptionSignal(
            {m_lyricBaseWidget->isVisible(), m_lyricExtWidget->isVisible(),
             m_lyricBaseWidget->fontSize(), m_lyricBaseWidget->skipSlur(),
             m_lyricBaseWidget->splitMode(), m_lyricExtWidget->fontSize(),
             m_exportLanguage});
    }
} // namespace FillLyric
