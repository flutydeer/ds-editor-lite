#include "Modules/FillLyric/LyricTab.h"

#include "Modules/FillLyric/Controls/CellList.h"
#include "Modules/FillLyric/Controls/LyricCell.h"

#include <algorithm>

#include <QFileDialog>
#include <QMessageBox>

#include "Modules/FillLyric/Utils/G2pService.h"

#include "Modules/FillLyric/Utils/SplitLyric.h"

namespace FillLyric {
    // R16: UI 层统一 QStringList，调用 G2pService/LyricSplitter 时转 std::vector<std::string>
    static std::vector<std::string> toStdVector(const QStringList &qsl) {
        std::vector<std::string> v;
        v.reserve(static_cast<size_t>(qsl.size()));
        for (const auto &s : qsl) {
            const auto bytes = s.toUtf8();
            v.emplace_back(bytes.constData(), static_cast<size_t>(bytes.size()));
        }
        return v;
    }

    LyricTab::LyricTab(const QList<LangNote> &langNotes, SingerIdentifier singer,
                       const srt::g2p::LanguageService &languageService,
                       LyricLanguageContextPtr languageContext, const LyricTabConfig &config,
                       QWidget *parent)
        : QWidget(parent), m_g2pService(std::move(singer), languageService) {
        m_languageContext = std::move(languageContext);
        for (const auto &langNote : langNotes) {
            m_langNotes.append(new LangNote(langNote));
            m_declaredLanguages.append(langNote.language);
        }

        m_lyricBaseWidget = new LyricBaseWidget(config, m_languageContext, this);
        m_lyricExtWidget =
            new LyricExtWidget(&m_notesCount, config, m_languageContext, &m_g2pService);

        m_lyricLayout = new QHBoxLayout();
        m_lyricLayout->setContentsMargins(0, 0, 0, 0);
        m_lyricLayout->addWidget(m_lyricBaseWidget, 1);
        m_lyricLayout->addWidget(m_lyricExtWidget, 2);

        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->setContentsMargins(0, 10, 0, 10);
        m_mainLayout->addLayout(m_lyricLayout);

        connect(m_lyricBaseWidget, &LyricBaseWidget::modifyOption, this, &LyricTab::modifyOption);
        connect(m_lyricExtWidget, &LyricExtWidget::modifyOption, this, &LyricTab::modifyOption);

        connect(m_lyricBaseWidget, &LyricBaseWidget::reReadNoteRequested, this,
                [this] { setLangNotes(false); });
        connect(m_lyricBaseWidget, &LyricBaseWidget::toTableRequested, this,
                &LyricTab::onBtnToTableClicked);
        connect(m_lyricExtWidget, &LyricExtWidget::insertTextRequested, this,
                &LyricTab::onBtnInsertTextClicked);

        connect(m_lyricBaseWidget, &LyricBaseWidget::lyricPrevRequested, this, [this] {
            m_lyricExtWidget->setVisible(!m_lyricExtWidget->isVisible());
            if (m_lyricExtWidget->isVisible() && !m_previewInitialized)
                initializeOriginalNotesPreview();
            m_lyricBaseWidget->setToTableVisible(m_lyricExtWidget->isVisible());
            m_lyricBaseWidget->setLyricPrevText(m_lyricExtWidget->isVisible() ? tr("Fold Preview")
                                                                              : tr("Lyric Prev"));
            if (m_lyricExtWidget->isVisible()) {
                Q_EMIT this->expandWindowRight();
            } else {
                Q_EMIT this->shrinkWindowRight();
            }
            modifyOption();
        });

        connect(m_lyricExtWidget, &LyricExtWidget::foldLeftRequested, this, [this] {
            m_lyricBaseWidget->setVisible(!m_lyricBaseWidget->isVisible());
            m_lyricExtWidget->setFoldLeftText(m_lyricBaseWidget->isVisible() ? tr("Fold Left")
                                                                             : tr("Expand Left"));
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
        connect(m_lyricBaseWidget, &LyricBaseWidget::splitOptionChanged, this,
                [this] { setLangNotes(false); });
        connect(m_lyricBaseWidget, &LyricBaseWidget::languageChoiceChanged, this,
                &LyricTab::onLanguageChoiceChanged);
    }

    LyricTab::~LyricTab() {
        qDeleteAll(m_langNotes);
        m_langNotes.clear();
    }

    const LyricLanguageContext &LyricTab::languageContext() const {
        return *m_languageContext;
    }

    void LyricTab::resetOriginalNotesG2p() {
        // Only the derived conversion output is rebuilt. The language a note
        // declares and the pronunciation it edited are user state, so a rebuilt
        // preview must not re-detect them.
        if (m_declaredLanguages.size() != m_langNotes.size())
            m_declaredLanguages.clear();
        for (qsizetype i = 0; i < m_langNotes.size(); ++i) {
            auto *note = m_langNotes.at(i);
            note->language = m_declaredLanguages.value(static_cast<int>(i), note->language);
            note->g2pId = kUnknownG2pId;
            note->syllable.clear();
            note->candidates.clear();
        }
        m_originalNotesG2pReady = false;
    }

    void LyricTab::ensureOriginalNotesG2p() {
        if (m_originalNotesG2pReady)
            return;

        QList<LangNote> inputNotes;
        inputNotes.reserve(m_langNotes.size());
        for (const auto &note : m_langNotes)
            inputNotes.append(*note);

        const auto g2pResults = m_g2pService.convert(inputNotes, languageContext());
        const int resultCount =
            std::min(static_cast<int>(m_langNotes.size()), static_cast<int>(g2pResults.size()));
        for (int i = 0; i < resultCount; i++) {
            if (m_langNotes[i]->language == QStringLiteral("unknown"))
                m_langNotes[i]->language = g2pResults[i].language;
            if (m_langNotes[i]->g2pId == kUnknownG2pId || m_langNotes[i]->g2pId.isEmpty())
                m_langNotes[i]->g2pId = g2pResults[i].g2pId;
            m_langNotes[i]->syllable = g2pResults[i].pronunciation;
            m_langNotes[i]->candidates = g2pResults[i].candidates;
        }
        m_originalNotesG2pReady = true;
    }

    void LyricTab::initializeOriginalNotesPreview() {
        ensureOriginalNotesG2p();

        QList<LangNote> langNotes;
        for (const auto &langNote : m_langNotes) {
            if (m_lyricBaseWidget->skipSlur() &&
                (langNote->g2pId == kSlurLyric || langNote->lyric == "-"))
                continue;
            langNotes.append(*langNote);
        }

        m_notesCount = static_cast<int>(langNotes.size());
        m_lyricExtWidget->wrapView()->init({langNotes});
        m_previewInitialized = true;
    }

    void LyricTab::onLanguageChoiceChanged() {
        if (!confirmPreviewRebuild()) {
            m_lyricBaseWidget->revertLanguageChoice();
            Q_EMIT languageChoiceChanged();
            return;
        }
        setLangNotes(false);
        Q_EMIT languageChoiceChanged();
    }

    bool LyricTab::confirmPreviewRebuild() const {
        if (!m_lyricExtWidget->isVisible() || !previewHasRevisions())
            return true;
        const auto answer = QMessageBox::question(
            nullptr, tr("Preview Lyric"),
            tr("Changing the language rebuilds the preview and discards your revisions. "
               "Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        return answer == QMessageBox::Yes;
    }

    bool LyricTab::previewHasRevisions() const {
        for (const auto *cellList : m_lyricExtWidget->wrapView()->cellLists()) {
            for (const auto *cell : cellList->m_cells) {
                if (cell->note()->revised)
                    return true;
            }
        }
        return false;
    }

    void LyricTab::setLangNotes(const bool warn) {
        const bool skipSlurRes = m_lyricBaseWidget->skipSlur();

        bool setLangNotes = false;
        if (warn) {
            const QMessageBox::StandardButton res = QMessageBox::question(
                nullptr, tr("Preview Lyric"), tr("Split the lyric into Preview window?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (res == QMessageBox::Yes)
                setLangNotes = true;
        }

        if (!warn || setLangNotes) {
            QStringList lyrics;
            const auto currentText = m_lyricBaseWidget->lyricText();
            if (currentText.trimmed().isEmpty()) {
                // The dialog opens before the text is populated, so the first
                // rebuild starts from the notes it was opened with.
                for (const auto &langNote : m_langNotes) {
                    if (skipSlurRes && (langNote->g2pId == kSlurLyric || langNote->lyric == "-"))
                        continue;
                    lyrics.append(langNote->lyric);
                }
            } else {
                // Re-split the text on screen, so lyrics typed since the last
                // split survive a language change instead of being replaced by
                // the notes they were derived from.
                qDeleteAll(m_langNotes);
                m_langNotes.clear();
                m_declaredLanguages.clear();
                const auto split = m_lyricBaseWidget->splitLyric(currentText);
                for (const auto &line : split) {
                    for (const auto &langNote : line) {
                        m_langNotes.append(new LangNote(langNote));
                        m_declaredLanguages.append(langNote.language);
                        if (skipSlurRes &&
                            (langNote.g2pId == kSlurLyric || langNote.lyric == "-"))
                            continue;
                        lyrics.append(langNote.lyric);
                    }
                }
            }
            m_notesCount = static_cast<int>(lyrics.size());
            m_lyricBaseWidget->setLyricText(lyrics.join(" "));
            m_previewInitialized = false;
            resetOriginalNotesG2p();
            if (m_lyricExtWidget->isVisible())
                initializeOriginalNotesPreview();
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
            const auto g2pResults = m_g2pService.convert(notes, languageContext());
            for (int i = 0; i < g2pResults.size(); i++) {
                if (notes[i].language == QStringLiteral("unknown"))
                    notes[i].language = g2pResults[i].language;
                if (notes[i].g2pId == kUnknownG2pId || notes[i].g2pId.isEmpty())
                    notes[i].g2pId = g2pResults[i].g2pId;
                notes[i].syllable = g2pResults[i].pronunciation;
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

    bool LyricTab::exportSkipSlur() const {
        return m_lyricBaseWidget->skipSlur();
    }

    void LyricTab::onBtnInsertTextClicked() {
        const QString text = m_lyricBaseWidget->lyricText();
        if (text.isEmpty())
            return;
        m_lyricExtWidget->wrapView()->init(
            LyricSplitter::splitAuto(text, toStdVector(languageContext().priorityLanguages())));
        m_previewInitialized = true;
    }

    void LyricTab::onBtnToTableClicked() {
        const QString text = m_lyricBaseWidget->lyricText();
        const auto splitRes = m_lyricBaseWidget->splitLyric(text);

        const QMessageBox::StandardButton res = QMessageBox::question(
            nullptr, tr("Preview Lyric"), tr("Split the lyric into Preview window?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (res == QMessageBox::Yes) {
            m_lyricExtWidget->wrapView()->init(splitRes);
            m_previewInitialized = true;
        }
    }

    void LyricTab::modifyOption() {
        Q_EMIT this->modifyOptionSignal(
            {m_lyricBaseWidget->isVisible(), m_lyricExtWidget->isVisible(),
             m_lyricBaseWidget->fontSize(), m_lyricBaseWidget->skipSlur(),
             m_lyricBaseWidget->splitMode(), m_lyricExtWidget->fontSize()});
    }
} // namespace FillLyric
