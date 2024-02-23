#include "LyricDialog.h"

#include <QApplication>

namespace FillLyric {

    LyricDialog::LyricDialog(QList<Note *> note, QWidget *parent)
        : Dialog(parent), m_notes(std::move(note)) {
        setWindowTitle("Fill Lyric");
        setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
        // 窗口大小设为主程序的80%
        auto size = QApplication::primaryScreen()->availableSize();
        resize((int) (size.width() * 0.6), (int) (size.height() * 0.6));

        noteToPhonic();

        m_mainLayout = new QVBoxLayout(this);
        m_tabWidget = new QTabWidget();

        m_lyricWidget = new LyricWidget(m_phonicNotes);

        m_btnOk = new Button("OK", this);
        m_btnOk->setPrimary(true);
        m_btnCancel = new Button("Cancel", this);

        m_btnOk->setMaximumWidth(100);
        m_btnCancel->setMaximumWidth(100);

        auto buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch(1);
        buttonLayout->addWidget(m_btnOk);
        buttonLayout->addWidget(m_btnCancel);

        m_tabWidget->addTab(m_lyricWidget, "Lyric");
        m_tabWidget->addTab(new QWidget, "Advanced");
        m_tabWidget->addTab(new QWidget, "Help");

        m_mainLayout->addWidget(m_tabWidget);
        m_mainLayout->addLayout(buttonLayout);

        connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
        connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

        connect(m_lyricWidget, &LyricWidget::shrinkWindowRight, this,
                &LyricDialog::shrinkWindowRight);
        connect(m_lyricWidget, &LyricWidget::expandWindowRight, this,
                &LyricDialog::expandWindowRight);
    }

    LyricDialog::~LyricDialog() = default;

    void LyricDialog::noteToPhonic() {
        for (auto note : m_notes) {
            auto lyric = note->lyric();
            auto syllable = Pron(note->pronunciation().original, note->pronunciation().edited);
            auto lineFeed = note->lineFeed();

            auto phonicNote = new PhonicNote(lyric, syllable, lineFeed);
            if (note->isSlur())
                phonicNote->setLyricType(TextType::Slur);

            m_phonicNotes.append(phonicNote);
        }
    }

    void LyricDialog::phonicToNote() {
        for (int i = 0; i < m_phonicNotes.size(); ++i) {
            auto note = m_notes.at(i);
            auto phonicNote = m_phonicNotes.at(i);

            note->setLyric(phonicNote->lyric());
            note->setPronunciation(Pronunciation(phonicNote->pronunciation().original,
                                                 phonicNote->pronunciation().edited));
            note->pronCandidates() = phonicNote->pronCandidates();
            note->setLineFeed(phonicNote->lineFeed());
        }
    }

    void LyricDialog::shrinkWindowRight(int newWidth) {
        setMinimumSize(300, 450);
        resize(newWidth, height());
    }

    void LyricDialog::expandWindowRight() {
        setMinimumSize(720, 450);
        auto size = QApplication::primaryScreen()->availableSize();
        resize(int(size.width() * 0.6), height());
    }


} // FillLyric