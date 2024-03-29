#include "LyricBaseWidget.h"

#include <QFileDialog>
#include <QMessageBox>

#include "Model/AppOptions/AppOptions.h"

#include "../../Utils/SplitLyric.h"
#include "../../Utils/LrcTools/LrcDecoder.h"

namespace FillLyric {
    LyricBaseWidget::LyricBaseWidget(QWidget *parent) : QWidget(parent) {
        // textEdit top
        m_textTopLayout = new QHBoxLayout();
        btnImportLrc = new Button(tr("Import Lrc"));
        btnReReadNote = new Button(tr("Reread Note"));
        btnLyricPrev = new Button(tr("Lyric Prev"));
        m_textTopLayout->addWidget(btnImportLrc);
        m_textTopLayout->addWidget(btnReReadNote);
        m_textTopLayout->addStretch(1);
        m_textTopLayout->addWidget(btnLyricPrev);

        // textEdit
        m_textEdit = new PhonicTextEdit();
        m_textEdit->setPlaceholderText(tr("Please input lyric here."));

        m_textBottomLayout = new QHBoxLayout();
        m_textCountLabel = new QLabel(tr("Note Count: 0"));

        m_btnToTable = new Button(">>");
        m_btnToTable->setFixedSize(40, 20);

        m_textBottomLayout->addWidget(m_textCountLabel);
        m_textBottomLayout->addStretch(1);
        m_textBottomLayout->addWidget(m_btnToTable);

        m_mainLayout = new QVBoxLayout();
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->addLayout(m_textTopLayout);
        m_mainLayout->addWidget(m_textEdit);
        m_mainLayout->addLayout(m_textBottomLayout);

        m_optLabelLayout = new QHBoxLayout();
        m_optLabel = new QLabel(tr("Fill-in Options:"));
        m_optButton = new QPushButton();
        m_optButton->setFixedSize(20, 20);
        m_optButton->setIcon(QIcon(":/svg/icons/chevron_down_16_filled_white.svg"));

        m_optLabelLayout->addWidget(m_optLabel);
        m_optLabelLayout->addStretch(1);
        m_optLabelLayout->addWidget(m_optButton);
        m_mainLayout->addLayout(m_optLabelLayout);

        m_optWidget = new QWidget();
        m_optWidget->setContentsMargins(0, 0, 0, 0);

        // bottom layout
        m_splitLayout = new QHBoxLayout();
        m_splitLayout->setContentsMargins(0, 0, 0, 0);
        m_splitLabel = new QLabel(tr("Split Mode :"));
        m_splitComboBox = new ComboBox(true);
        m_splitComboBox->addItems({tr("Auto"), tr("By Char"), tr("Custom")});
        m_splitters = new LineEdit();
        m_splitters->setVisible(false);
        m_splitters->setMaximumWidth(85);
        m_splitLayout->addWidget(m_splitLabel);
        m_splitLayout->addWidget(m_splitComboBox);
        m_splitLayout->addWidget(m_splitters);
        m_splitLayout->addStretch(1);

        skipSlur = new QCheckBox(tr("Skip Slur Note"));
        m_skipSlurLayout = new QHBoxLayout();
        m_skipSlurLayout->addWidget(skipSlur);
        m_skipSlurLayout->addStretch(1);

        m_optLayout = new QVBoxLayout();
        m_optLayout->addLayout(m_splitLayout);
        m_optLayout->addLayout(m_skipSlurLayout);
        m_optWidget->setLayout(m_optLayout);
        m_mainLayout->addWidget(m_optWidget);

        this->setLayout(m_mainLayout);

        const auto options = AppOptions::instance()->fillLyric();
        auto font = m_textEdit->font();
        font.setPointSizeF(options->textEditFontSize);
        m_textEdit->setFont(font);
        skipSlur->setChecked(options->skipSlur);
        m_splitComboBox->setCurrentIndex(options->splitMode);
        m_splitters->setVisible(options->splitMode == Custom);

        // textEditTop signals
        connect(btnImportLrc, &QAbstractButton::clicked, this,
                &LyricBaseWidget::_on_btnImportLrc_clicked);

        // textEdit label
        connect(m_textEdit, &PhonicTextEdit::textChanged, this,
                &LyricBaseWidget::_on_textEditChanged);

        connect(m_textEdit, &PhonicTextEdit::fontChanged, this, &LyricBaseWidget::modifyOption);

        connect(m_splitComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &LyricBaseWidget::_on_splitComboBox_currentIndexChanged);

        connect(m_optButton, &QPushButton::clicked,
                [this]() { m_optWidget->setVisible(!m_optWidget->isVisible()); });

        connect(skipSlur, &QCheckBox::stateChanged, this, &LyricBaseWidget::modifyOption);
    }

    LyricBaseWidget::~LyricBaseWidget() = default;

    void LyricBaseWidget::_on_textEditChanged() const {
        const QString text = this->m_textEdit->toPlainText();
        const auto splitRes = this->splitLyric(text);
        int count = 0;
        for (const auto &notes : splitRes) {
            count += static_cast<int>(notes.size());
        }
        this->m_textCountLabel->setText(QString(tr("Note Count: %1")).arg(count));
    }

    void LyricBaseWidget::_on_btnImportLrc_clicked() {
        const QString fileName =
            QFileDialog::getOpenFileName(this, tr("Open Lrc File"), "", tr("Lrc Files (*.lrc)"));
        if (fileName.isEmpty()) {
            return;
        }

        LrcTools::LrcDecoder decoder;
        if (!decoder.decode(fileName)) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to decode lrc file."));
            return;
        }

        const auto metadata = decoder.dumpMetadata();
        qDebug() << "metadata: " << metadata;

        const auto lyrics = decoder.dumpLyrics();
        this->m_textEdit->setPlainText(lyrics.join("\n"));
    }

    QList<QList<LangNote>> LyricBaseWidget::splitLyric(const QString &lyric) const {
        const bool skipSlurRes = this->skipSlur->isChecked();
        const auto splitType = static_cast<SplitType>(this->m_splitComboBox->currentIndex());

        QList<QList<LangNote>> splitNotes;
        if (splitType == Auto) {
            splitNotes = CleanLyric::splitAuto(lyric);
        } else if (splitType == ByChar) {
            splitNotes = CleanLyric::splitByChar(lyric);
        } else if (splitType == Custom) {
            splitNotes = CleanLyric::splitCustom(lyric, this->m_splitters->text().split(' '));
        }

        QList<QList<LangNote>> skipSlurLangNotes;
        if (skipSlurRes) {
            for (const auto &notes : splitNotes) {
                QList<LangNote> tempNotes;
                for (const auto &note : notes) {
                    if (note.language != "Slur" && note.lyric != "-") {
                        LangNote langNote;
                        langNote.lyric = note.lyric;
                        langNote.syllable = note.syllable;
                        langNote.candidates = note.candidates;
                        langNote.language = note.language;
                        langNote.category = note.category;
                        tempNotes.append(langNote);
                    }
                }
                skipSlurLangNotes.append(tempNotes);
            }
        }

        const auto res = skipSlurRes ? skipSlurLangNotes : splitNotes;
        return res;
    }

    void LyricBaseWidget::_on_splitComboBox_currentIndexChanged(int index) const {
        const auto splitType = static_cast<SplitType>(index);
        m_splitters->setVisible(splitType == Custom);
        modifyOption();
    }

    void LyricBaseWidget::modifyOption() const {
        const auto options = AppOptions::instance()->fillLyric();
        options->textEditFontSize = m_textEdit->font().pointSizeF();
        options->skipSlur = skipSlur->isChecked();
        options->splitMode = m_splitComboBox->currentIndex();
        AppOptions::instance()->saveAndNotify();
    }

} // FillLyric