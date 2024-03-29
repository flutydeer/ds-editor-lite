#include "LyricDialog.h"

#include <QApplication>

#include "Model/AppOptions/AppOptions.h"

LyricDialog::LyricDialog(QList<Note *> note, QWidget *parent)
    : Dialog(parent), m_notes(std::move(note)) {
    setModal(true);
    setMinimumSize(720, 450);
    setWindowTitle(tr("Fill Lyric"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);

    const auto size = QApplication::primaryScreen()->availableSize();
    resize(static_cast<int>(size.width() * 0.6), static_cast<int>(size.height() * 0.6));

    noteToPhonic();

    m_mainLayout = new QVBoxLayout();
    m_tabWidget = new QTabWidget();

    m_lyricWidget = new FillLyric::LyricTab(m_langNotes);
    m_lyricWidget->setLangNotes();

    if (!AppOptions::instance()->fillLyric()->extVisible) {
        shrinkWindowRight(300);
    }

    m_langPage = new LanguagePage(this);

    m_btnOk = new Button(tr("&Import"), this);
    // m_btnOk->setPrimary(true);
    setPositiveButton(m_btnOk);
    m_btnCancel = new Button(tr("&Cancel"), this);
    setNegativeButton(m_btnCancel);

    m_tabWidget->addTab(m_lyricWidget, tr("Lyric"));
    m_tabWidget->addTab(m_langPage, tr("Advanced"));
    m_tabWidget->addTab(new QWidget, tr("Help"));

    m_mainLayout->addWidget(m_tabWidget);
    m_mainLayout->setContentsMargins({});
    body()->setLayout(m_mainLayout);

    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_lyricWidget, &FillLyric::LyricTab::shrinkWindowRight, this,
            &LyricDialog::shrinkWindowRight);
    connect(m_lyricWidget, &FillLyric::LyricTab::expandWindowRight, this,
            &LyricDialog::expandWindowRight);

    connect(m_tabWidget, &QTabWidget::currentChanged, this, &LyricDialog::switchTab);
}

LyricDialog::~LyricDialog() = default;

void LyricDialog::noteToPhonic() {
    for (const auto note : m_notes) {
        const auto langNote = new LangNote();
        langNote->lyric = note->lyric();
        langNote->category = note->language();
        langNote->syllable = note->pronunciation().original;
        langNote->syllableRevised = note->pronunciation().edited;
        langNote->candidates = note->pronCandidates();

        if (note->isSlur()) {
            langNote->language = "Slur";
            langNote->category = "Slur";
        }

        m_langNotes.append(langNote);
    }
}

void LyricDialog::shrinkWindowRight(const int &newWidth) {
    setMinimumSize(300, 450);
    resize(newWidth, height());
}

void LyricDialog::expandWindowRight() {
    setMinimumSize(720, 450);
    const auto size = QApplication::primaryScreen()->availableSize();
    resize(static_cast<int>(size.width() * 0.6), height());
}

void LyricDialog::exportLangNotes() {
    const auto noteLists = m_lyricWidget->exportLangNotes();

    const bool skipSlurRes = m_lyricWidget->exportSkipSlur();

    const bool exportLangRes = m_lyricWidget->exportLanguage();

    QList<Note *> notes;
    for (const auto note : m_notes) {
        if (skipSlurRes && note->isSlur())
            continue;
        notes.append(note);
    }

    int count = 0;
    for (const auto &langNotes : noteLists) {
        if (count >= notes.size())
            break;
        for (const auto &langNote : langNotes) {
            if (count >= notes.size())
                break;
            const auto note = notes.at(count);
            note->setLyric(langNote.lyric);
            note->setPronunciation(Pronunciation(langNote.syllable, langNote.syllableRevised));
            note->setPronCandidates(langNote.candidates);
            if (exportLangRes && note->language() == "Unknown") {
                note->setLanguage(langNote.category);
            }
            count++;
        }
        if (count >= 1)
            notes.at(count - 1)->setLineFeed(true);
    }
}

void LyricDialog::switchTab(const int &index) {
    if (index == 0) {
        if (!m_lyricWidget->m_lyricExtWidget->isVisible()) {
            this->shrinkWindowRight(300);
        }
    } else {
        if (!m_lyricWidget->m_lyricExtWidget->isVisible()) {
            this->expandWindowRight();
        }
    }
}