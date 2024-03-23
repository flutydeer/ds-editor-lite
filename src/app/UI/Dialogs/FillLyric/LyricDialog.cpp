#include "LyricDialog.h"

#include <QApplication>

LyricDialog::LyricDialog(QList<Note *> note, QWidget *parent)
    : Dialog(parent), m_notes(std::move(note)) {
    setModal(true);
    setMinimumSize(720, 450);
    setWindowTitle(tr("Fill Lyric"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    // 窗口大小设为主程序的80%
    const auto size = QApplication::primaryScreen()->availableSize();
    resize(static_cast<int>(size.width() * 0.6), static_cast<int>(size.height() * 0.6));

    noteToPhonic();

    m_mainLayout = new QVBoxLayout();
    m_tabWidget = new QTabWidget();

    m_lyricWidget = new FillLyric::LyricTab(m_phonics);
    m_lyricWidget->setPhonics();

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
}

LyricDialog::~LyricDialog() = default;

void LyricDialog::noteToPhonic() {
    for (const auto note : m_notes) {
        const auto phonic = new FillLyric::Phonic;
        phonic->lyric = note->lyric();
        phonic->syllable = note->pronunciation().original;
        phonic->syllableRevised = note->pronunciation().edited;
        phonic->candidates = note->pronCandidates();

        if (note->isSlur())
            phonic->language = "Slur";

        m_phonics.append(phonic);
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

void LyricDialog::exportPhonics() {
    const QList<FillLyric::Phonic> phonics = m_lyricWidget->exportPhonics();

    const bool skipSlurRes = m_lyricWidget->exportSkipSlur();

    const bool exportLangRes = m_lyricWidget->exportLanguage();

    QList<Note *> notes;
    for (const auto note : m_notes) {
        if (skipSlurRes && note->isSlur())
            continue;
        notes.append(note);
    }

    for (int i = 0; i < phonics.size(); ++i) {
        const auto &phonic = phonics.at(i);
        if (i >= notes.size())
            break;
        const auto note = notes.at(i);

        note->setLyric(phonic.lyric);
        note->setPronunciation(Pronunciation(phonic.syllable, phonic.syllableRevised));
        note->setPronCandidates(phonic.candidates);
        if (exportLangRes) {
            // TODO: set language
            // note->setLanguage(phonic.language);
        }
        note->setLineFeed(phonic.lineFeed);
    }
    }