#include "Modules/FillLyric/Widgets/LyricBaseWidget.h"

#include <QFileDialog>
#include <QMessageBox>

#include "Modules/FillLyric/Utils/LrcTools/LrcDecoder.h"
#include "Modules/FillLyric/Utils/SplitLyric.h"

namespace FillLyric
{
    LyricBaseWidget::LyricBaseWidget(const LyricTabConfig &config, std::vector<std::string> priorityG2pIds,
                                     QMap<std::string, std::string> langToG2pId, QWidget *parent) :
        QWidget(parent), m_priorityG2pIds(std::move(priorityG2pIds)), m_langToG2pId(std::move(langToG2pId)) {
        m_textTopLayout = new QHBoxLayout();
        m_btnImportLrc = new QPushButton(tr("Import Lrc"));
        m_btnReReadNote = new QPushButton(tr("Reread Note"));
        m_btnLyricPrev = new QPushButton(tr("Lyric Prev"));
        m_textTopLayout->addWidget(m_btnImportLrc);
        m_textTopLayout->addWidget(m_btnReReadNote);
        m_textTopLayout->addStretch(1);
        m_textTopLayout->addWidget(m_btnLyricPrev);

        m_textEdit = new PhonicTextEdit();
        m_textEdit->setPlaceholderText(tr("Please input lyric here."));

        m_textBottomLayout = new QHBoxLayout();
        m_textCountLabel = new QLabel(tr("Note Count: 0"));

        m_btnToTable = new QPushButton(">>");
        m_btnToTable->setToolTip(tr("split lyric to preview dialog"));
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
        m_optLabel->setBuddy(m_optButton);
        m_optButton->setFixedSize(20, 20);
        m_optButton->setIcon(QIcon(":/svg/icons/chevron_down_16_filled_white.svg"));

        m_optLabelLayout->addWidget(m_optLabel);
        m_optLabelLayout->addStretch(1);
        m_optLabelLayout->addWidget(m_optButton);
        m_mainLayout->addLayout(m_optLabelLayout);

        m_optWidget = new QWidget();
        m_optWidget->setContentsMargins(0, 0, 0, 0);

        m_splitLayout = new QHBoxLayout();
        m_splitLayout->setContentsMargins(0, 0, 0, 0);
        m_splitLabel = new QLabel(tr("Split Mode :"));
        m_splitComboBox = new QComboBox();
        m_splitLabel->setBuddy(m_splitComboBox);
        m_splitComboBox->addItems({tr("Auto"), tr("By Char"), tr("Custom")});
        m_splitters = new QLineEdit();
        m_splitters->setToolTipDuration(0);
        m_splitters->setVisible(false);
        m_splitters->setMaximumWidth(85);
        m_splitLayout->addWidget(m_splitLabel);
        m_splitLayout->addWidget(m_splitComboBox);
        m_splitLayout->addWidget(m_splitters);
        m_splitLayout->addStretch(1);

        m_skipSlur = new QCheckBox(tr("Skip Slur Note"));
        m_skipSlurLayout = new QHBoxLayout();
        m_skipSlurLayout->addWidget(m_skipSlur);
        m_skipSlurLayout->addStretch(1);

        m_optLayout = new QVBoxLayout();
        m_optLayout->addLayout(m_splitLayout);
        m_optLayout->addLayout(m_skipSlurLayout);
        m_optWidget->setLayout(m_optLayout);
        m_mainLayout->addWidget(m_optWidget);

        this->setLayout(m_mainLayout);

        auto font = m_textEdit->font();
        font.setPointSizeF(std::max(9.0, config.lyricBaseFontSize));
        m_textEdit->setFont(font);
        m_splitComboBox->setCurrentIndex(config.splitMode);
        m_splitters->setVisible(config.splitMode == Custom);

        connect(m_btnImportLrc, &QAbstractButton::clicked, this, &LyricBaseWidget::onBtnImportLrcClicked);

        connect(m_btnReReadNote, &QAbstractButton::clicked, this, &LyricBaseWidget::reReadNoteRequested);
        connect(m_btnToTable, &QAbstractButton::clicked, this, &LyricBaseWidget::toTableRequested);
        connect(m_btnLyricPrev, &QAbstractButton::clicked, this, &LyricBaseWidget::lyricPrevRequested);

        connect(m_textEdit, &PhonicTextEdit::textChanged, this, &LyricBaseWidget::onTextEditChanged);

        connect(m_textEdit, &PhonicTextEdit::fontChanged, this, &LyricBaseWidget::modifyOption);

        connect(m_splitComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &LyricBaseWidget::onSplitComboBoxCurrentIndexChanged);

        connect(m_optButton, &QPushButton::clicked, this,
                [this] { m_optWidget->setVisible(!m_optWidget->isVisible()); });

        connect(m_skipSlur, &QCheckBox::stateChanged, this, [this] {
            Q_EMIT modifyOption();
            Q_EMIT splitOptionChanged();
        });
    }

    LyricBaseWidget::~LyricBaseWidget() = default;

    QString LyricBaseWidget::lyricText() const { return m_textEdit->toPlainText(); }

    void LyricBaseWidget::setLyricText(const QString &text) { m_textEdit->setPlainText(text); }

    bool LyricBaseWidget::skipSlur() const { return m_skipSlur->isChecked(); }

    void LyricBaseWidget::setSkipSlur(bool skip) { m_skipSlur->setChecked(skip); }

    int LyricBaseWidget::splitMode() const { return m_splitComboBox->currentIndex(); }

    QString LyricBaseWidget::splitters() const { return m_splitters->text(); }

    double LyricBaseWidget::fontSize() const { return m_textEdit->font().pointSizeF(); }

    void LyricBaseWidget::setToTableVisible(bool visible) { m_btnToTable->setVisible(visible); }

    void LyricBaseWidget::setLyricPrevText(const QString &text) { m_btnLyricPrev->setText(text); }

    void LyricBaseWidget::onTextEditChanged() const {
        const QString text = this->m_textEdit->toPlainText();
        const auto splitRes = this->splitLyric(text);
        int count = 0;
        for (const auto &notes : splitRes) {
            count += static_cast<int>(notes.size());
        }
        this->m_textCountLabel->setText(tr("Note Count: ") + QString::number(count));
    }

    void LyricBaseWidget::onBtnImportLrcClicked() {
        const QString fileName = QFileDialog::getOpenFileName(this, tr("Open Lrc File"), "", tr("Lrc Files (*.lrc)"));
        if (fileName.isEmpty()) {
            return;
        }

        LrcTools::LrcDecoder decoder;
        if (!decoder.decode(fileName)) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to decode lrc file."));
            return;
        }

        const auto metadata = decoder.dumpMetadata();

        const auto lyrics = decoder.dumpLyrics();
        this->m_textEdit->setPlainText(lyrics.join("\n"));
    }

    QList<QList<LangNote>> LyricBaseWidget::splitLyric(const QString &lyric) const {
        const auto splitType = static_cast<SplitType>(this->m_splitComboBox->currentIndex());

        QList<QList<LangNote>> splitNotes;
        if (splitType == Auto) {
            splitNotes = LyricSplitter::splitAuto(lyric, m_priorityG2pIds);
        } else if (splitType == ByChar) {
            splitNotes = LyricSplitter::splitByChar(lyric);
        } else if (splitType == Custom) {
            splitNotes = LyricSplitter::splitCustom(lyric, this->m_splitters->text().split(' '));
        }

        return splitNotes;
    }

    void LyricBaseWidget::onSplitComboBoxCurrentIndexChanged(int index) const {
        const auto splitType = static_cast<SplitType>(index);
        m_splitters->setVisible(splitType == Custom);
        this->onTextEditChanged();
        Q_EMIT modifyOption();
        Q_EMIT splitOptionChanged();
    }

} // namespace FillLyric
