#include "LyricDialog.h"

#include <QApplication>

namespace FillLyric {

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

        m_mainLayout = new QVBoxLayout(this);
        m_tabWidget = new QTabWidget();

        m_lyricWidget = new LyricTab(m_phonics);
        m_lyricWidget->setPhonics();

        m_btnOk = new Button(tr("OK"), this);
        m_btnOk->setPrimary(true);
        m_btnCancel = new Button(tr("Cancel"), this);

        m_btnOk->setMaximumWidth(100);
        m_btnCancel->setMaximumWidth(100);

        const auto buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch(1);
        buttonLayout->addWidget(m_btnOk);
        buttonLayout->addWidget(m_btnCancel);

        m_tabWidget->addTab(m_lyricWidget, tr("Lyric"));
        m_tabWidget->addTab(new QWidget, tr("Advanced"));
        m_tabWidget->addTab(new QWidget, tr("Help"));

        m_mainLayout->addWidget(m_tabWidget);
        m_mainLayout->addLayout(buttonLayout);

        connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
        connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

        connect(m_lyricWidget, &LyricTab::shrinkWindowRight, this, &LyricDialog::shrinkWindowRight);
        connect(m_lyricWidget, &LyricTab::expandWindowRight, this, &LyricDialog::expandWindowRight);
    }

    LyricDialog::~LyricDialog() = default;

    void LyricDialog::noteToPhonic() {
        for (const auto note : m_notes) {
            const auto phonic = new Phonic;
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
        const QList<Phonic> phonics = m_lyricWidget->exportPhonics();

        const bool skipSlurRes = m_lyricWidget->exportSkipSlur();

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
            note->setLineFeed(phonic.lineFeed);
        }
    }


} // FillLyric