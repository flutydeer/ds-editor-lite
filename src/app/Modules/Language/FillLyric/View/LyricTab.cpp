#include "LyricTab.h"

#include <QFileDialog>

#include "UI/Controls/LineEdit.h"
#include "Model/AppOptions/AppOptions.h"

#include "../Utils/SplitLyric.h"
#include "LangMgr/ILanguageManager.h"
#include "LangMgr/LangAnalysis/CantoneseAnalysis.h"

namespace FillLyric {

    LyricTab::LyricTab(QList<LangNote> langNotes, QWidget *parent)
        : QWidget(parent), m_langNotes(std::move(langNotes)) {
        // textWidget
        m_lyricBaseWidget = new LyricBaseWidget();

        // lyricExtWidget
        m_lyricExtWidget = new LyricExtWidget(&notesCount);

        // lyric layout
        m_lyricLayout = new QHBoxLayout();
        m_lyricLayout->setContentsMargins(0, 0, 0, 0);
        m_lyricLayout->addWidget(m_lyricBaseWidget, 1);
        m_lyricLayout->addWidget(m_lyricExtWidget, 2);

        // main layout
        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->setContentsMargins(0, 10, 0, 10);
        m_mainLayout->addLayout(m_lyricLayout);

        connect(m_lyricBaseWidget->btnReReadNote, &QAbstractButton::clicked, this,
                &LyricTab::setLangNotes);

        // phonicWidget signals
        connect(m_lyricExtWidget->m_btnInsertText, &QAbstractButton::clicked, this,
                &LyricTab::_on_btnInsertText_clicked);
        connect(m_lyricBaseWidget->m_btnToTable, &QAbstractButton::clicked, this,
                &LyricTab::_on_btnToTable_clicked);

        // fold right
        connect(m_lyricBaseWidget->btnLyricPrev, &QPushButton::clicked, [this]() {
            m_lyricExtWidget->setVisible(!m_lyricExtWidget->isVisible());
            m_lyricBaseWidget->btnLyricPrev->setText(
                m_lyricExtWidget->isVisible() ? tr("Fold Preview") : tr("Lyric Prev"));
            m_lyricBaseWidget->m_btnToTable->setVisible(m_lyricExtWidget->isVisible());

            if (!m_lyricExtWidget->isVisible()) {
                Q_EMIT this->shrinkWindowRight(m_lyricBaseWidget->width() + 20);
            } else {
                Q_EMIT this->expandWindowRight();
            }
            modifyOption();
        });

        // fold left
        connect(m_lyricExtWidget->btnFoldLeft, &QPushButton::clicked, [this]() {
            m_lyricBaseWidget->setVisible(!m_lyricBaseWidget->isVisible());
            m_lyricExtWidget->btnFoldLeft->setText(
                m_lyricBaseWidget->isVisible() ? tr("Fold Left") : tr("Expand Left"));
            modifyOption();
        });

        const auto options = appOptions->fillLyric();
        const bool baseVisible = options->baseVisible;
        const bool extVisible = options->extVisible;

        if (!baseVisible) {
            m_lyricBaseWidget->setVisible(baseVisible);
            m_lyricExtWidget->btnFoldLeft->setText(tr("Expand Left"));
        }

        if (!extVisible) {
            m_lyricExtWidget->setVisible(extVisible);
            m_lyricBaseWidget->m_btnToTable->setVisible(extVisible);
            m_lyricBaseWidget->btnLyricPrev->setText(tr("Lyric Prev"));
        } else {
            m_lyricBaseWidget->btnLyricPrev->setText(tr("Fold Preview"));
        }
    }

    LyricTab::~LyricTab() = default;

    void LyricTab::setLangNotes() {
        const bool skipSlurRes = m_lyricBaseWidget->skipSlur->isChecked();

        QStringList lyrics;
        QList<LangNote> langNotes;
        for (const auto &langNote : m_langNotes) {
            if (skipSlurRes && (langNote.language == "Slur" || langNote.lyric == "-"))
                continue;
            langNotes.append(langNote);
            lyrics.append(langNote.lyric);
        }
        notesCount = static_cast<int>(langNotes.size());
        m_lyricBaseWidget->m_textEdit->setPlainText(lyrics.join(" "));
        m_lyricExtWidget->m_wrapView->init({langNotes});
    }

    QList<QList<LangNote>> LyricTab::exportLangNotes() const {
        if (m_lyricExtWidget->isVisible()) {
            return this->modelExport();
        }
        auto langNotes = m_lyricBaseWidget->
            splitLyric(m_lyricBaseWidget->m_textEdit->toPlainText());

        QList<QList<LangNote>> result;
        const auto langMgr = LangMgr::ILanguageManager::instance();
        for (auto &notes : langNotes) {
            QList<LangNote *> inputNotes;
            QList<LangNote> lineRes;

            for (auto &note : notes) {
                inputNotes.append(&note);
            }
            langMgr->correct(inputNotes);
            langMgr->convert(inputNotes);
            for (const auto &note : inputNotes) {
                lineRes.append(*note);
            }
            result.append(lineRes);
        }
        return langNotes;
    }

    bool LyricTab::exportSkipSlur() const {
        return m_lyricExtWidget->isVisible()
                   ? m_lyricExtWidget->exportSkipSlur->isChecked()
                   : m_lyricBaseWidget->skipSlur->isChecked();
    }

    bool LyricTab::exportLanguage() const {
        return m_lyricExtWidget->exportLanguage->isChecked();
    }

    QList<QList<LangNote>> LyricTab::modelExport() const {
        const auto cellLists = m_lyricExtWidget->m_wrapView->cellLists();
        const bool skipSlurRes = exportSkipSlur();

        QList<QList<LangNote>> noteList;
        for (const auto &cellList : cellLists) {
            QList<LangNote> notes;
            for (const auto &cell : cellList->m_cells) {
                const auto note = cell->note();
                if (skipSlurRes && (note->language != "Slur" || note->lyric == "-"))
                    continue;
                notes.append(*note);
            }
            noteList.append(notes);
        }
        return noteList;
    }

    void LyricTab::_on_btnInsertText_clicked() const {
        const QString text =
            "Halloween蝉声--陪かな伴着qwe行云流浪---\nka回-忆-开始132后安静遥望远方"
            "\n荒草覆没的古井--枯塘\n匀-散asdaw一缕过往\n";
        m_lyricBaseWidget->m_textEdit->setPlainText(text);
    }

    void LyricTab::_on_btnToTable_clicked() const {
        const auto skipSlurRes = m_lyricBaseWidget->skipSlur->isChecked();
        const auto splitType =
            static_cast<SplitType>(m_lyricBaseWidget->m_splitComboBox->currentIndex());

        QString text = m_lyricBaseWidget->m_textEdit->toPlainText();
        if (skipSlurRes) {
            text = text.remove("-");
        }

        QList<QList<LangNote>> splitRes;
        if (splitType == Auto) {
            splitRes = CleanLyric::splitAuto(text);
        } else if (splitType == ByChar) {
            splitRes = CleanLyric::splitByChar(text);
        } else if (splitType == Custom) {
            splitRes =
                CleanLyric::splitCustom(text, m_lyricBaseWidget->m_splitters->text().split(' '));
        }

        m_lyricExtWidget->m_wrapView->init(splitRes);
    }

    void LyricTab::modifyOption() const {
        const auto options = appOptions->fillLyric();
        options->baseVisible = m_lyricBaseWidget->isVisible();
        options->extVisible = m_lyricExtWidget->isVisible();
        appOptions->saveAndNotify();
    }
} // FillLyric