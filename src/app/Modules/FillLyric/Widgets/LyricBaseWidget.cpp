#include "Modules/FillLyric/Widgets/LyricBaseWidget.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QSet>
#include <QSignalBlocker>

#include "Modules/FillLyric/Utils/LrcTools/LrcDecoder.h"
#include "Modules/FillLyric/Utils/SplitLyric.h"
#include "Utils/LanguageDisplayUtils.h"
#include <lite/GUI/Utils/IconUtils.h>

namespace FillLyric {
    // R16: UI 层统一 QStringList，调用 LyricSplitter 时转 std::vector<std::string>
    static std::vector<std::string> toStdVector(const QStringList &qsl) {
        std::vector<std::string> v;
        v.reserve(static_cast<size_t>(qsl.size()));
        for (const auto &s : qsl) {
            const auto bytes = s.toUtf8();
            v.emplace_back(bytes.constData(), static_cast<size_t>(bytes.size()));
        }
        return v;
    }

    LyricBaseWidget::LyricBaseWidget(const LyricTabConfig &config,
                                     LyricLanguageContextPtr languageContext, QWidget *parent)
        : QWidget(parent), m_languageContext(std::move(languageContext)) {
        m_textTopLayout = new QHBoxLayout();
        m_btnImportLrc =
            new QPushButton(QCoreApplication::translate("LyricBaseWidget", "Import Lrc"));
        m_btnReReadNote =
            new QPushButton(QCoreApplication::translate("LyricBaseWidget", "Reread Note"));
        m_btnLyricPrev =
            new QPushButton(QCoreApplication::translate("LyricBaseWidget", "Lyric Prev"));
        m_textTopLayout->addWidget(m_btnImportLrc);
        m_textTopLayout->addWidget(m_btnReReadNote);
        m_textTopLayout->addStretch(1);
        m_textTopLayout->addWidget(m_btnLyricPrev);

        m_textEdit = new PhonicTextEdit();
        m_textEdit->setPlaceholderText(
            QCoreApplication::translate("LyricBaseWidget", "Please input lyric here."));

        m_textBottomLayout = new QHBoxLayout();
        m_textCountLabel =
            new QLabel(QCoreApplication::translate("LyricBaseWidget", "Note Count: %L1").arg(0));

        m_btnToTable = new QPushButton(">>");
        m_btnToTable->setToolTip(
            QCoreApplication::translate("LyricBaseWidget", "split lyric to preview dialog"));
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
        m_optLabel = new QLabel(QCoreApplication::translate("LyricBaseWidget", "Fill-in Options:"));
        m_optButton = new QPushButton();
        m_optLabel->setBuddy(m_optButton);
        m_optButton->setFixedSize(20, 20);
        const QSize optButtonIconSize(16, 16);
        const auto optButtonIconColor = m_optButton->palette().color(QPalette::ButtonText);
        m_optButton->setIcon(IconUtils::createTintedSvgIcon(
            QStringLiteral(":/svg/icons/chevron_down_16_regular.svg"), optButtonIconSize,
            optButtonIconColor, optButtonIconColor));
        m_optButton->setIconSize(optButtonIconSize);

        m_optLabelLayout->addWidget(m_optLabel);
        m_optLabelLayout->addStretch(1);
        m_optLabelLayout->addWidget(m_optButton);
        m_mainLayout->addLayout(m_optLabelLayout);

        m_optWidget = new QWidget();
        m_optWidget->setContentsMargins(0, 0, 0, 0);

        m_splitLayout = new QHBoxLayout();
        m_splitLayout->setContentsMargins(0, 0, 0, 0);
        m_splitLabel = new QLabel(QCoreApplication::translate("LyricBaseWidget", "Split Mode :"));
        m_splitComboBox = new ComboBox();
        m_splitLabel->setBuddy(m_splitComboBox);
        m_splitComboBox->addItems({
            QCoreApplication::translate("LyricBaseWidget", "Auto"),
            QCoreApplication::translate("LyricBaseWidget", "By Char"),
            QCoreApplication::translate("LyricBaseWidget", "Custom"),
        });
        m_splitters = new QLineEdit();
        m_splitters->setToolTipDuration(0);
        m_splitters->setVisible(false);
        m_splitters->setMaximumWidth(85);
        m_splitLayout->addWidget(m_splitLabel);
        m_splitLayout->addWidget(m_splitComboBox);
        m_splitLayout->addWidget(m_splitters);
        m_splitLayout->addStretch(1);

        m_languageLayout = new QHBoxLayout();
        m_languageLayout->setContentsMargins(0, 0, 0, 0);
        m_languageLabel =
            new QLabel(QCoreApplication::translate("LyricBaseWidget", "Lyric Language :"));
        m_languageComboBox = new ComboBox();
        m_languageLabel->setBuddy(m_languageComboBox);
        m_languageLayout->addWidget(m_languageLabel);
        m_languageLayout->addWidget(m_languageComboBox);
        m_languageLayout->addStretch(1);
        refreshLanguageCombo();

        m_skipSlur =
            new QCheckBox(QCoreApplication::translate("LyricBaseWidget", "Skip Slur Note"));
        m_skipSlurLayout = new QHBoxLayout();
        m_skipSlurLayout->addWidget(m_skipSlur);
        m_skipSlurLayout->addStretch(1);

        m_optLayout = new QVBoxLayout();
        m_optLayout->addLayout(m_splitLayout);
        m_optLayout->addLayout(m_languageLayout);
        m_optLayout->addLayout(m_skipSlurLayout);
        m_optWidget->setLayout(m_optLayout);
        m_mainLayout->addWidget(m_optWidget);

        this->setLayout(m_mainLayout);

        auto font = m_textEdit->font();
        font.setPointSizeF(std::max(9.0, config.lyricBaseFontSize));
        m_textEdit->setFont(font);
        m_splitComboBox->setCurrentIndex(config.splitMode);
        m_splitters->setVisible(config.splitMode == Custom);

        connect(m_btnImportLrc, &QAbstractButton::clicked, this,
                &LyricBaseWidget::onBtnImportLrcClicked);

        connect(m_btnReReadNote, &QAbstractButton::clicked, this,
                &LyricBaseWidget::reReadNoteRequested);
        connect(m_btnToTable, &QAbstractButton::clicked, this, &LyricBaseWidget::toTableRequested);
        connect(m_btnLyricPrev, &QAbstractButton::clicked, this,
                &LyricBaseWidget::lyricPrevRequested);

        connect(m_textEdit, &PhonicTextEdit::textChanged, this,
                &LyricBaseWidget::onTextEditChanged);

        connect(m_textEdit, &PhonicTextEdit::fontChanged, this, &LyricBaseWidget::modifyOption);

        connect(m_splitComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &LyricBaseWidget::onSplitComboBoxCurrentIndexChanged);

        connect(m_optButton, &QPushButton::clicked, this,
                [this] { m_optWidget->setVisible(!m_optWidget->isVisible()); });

        connect(m_languageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int index) {
                    if (!m_languageContext || index < 0)
                        return;
                    m_previousLanguageChoice = m_languageContext->choice();
                    m_languageContext->setChoice(m_languageComboBox->itemData(index).toString());
                    updateLanguageToolTip();
                    Q_EMIT languageChoiceChanged();
                    Q_EMIT modifyOption();
                });

        connect(m_skipSlur, &QCheckBox::stateChanged, this, [this] {
            Q_EMIT modifyOption();
            Q_EMIT splitOptionChanged();
        });
    }

    LyricBaseWidget::~LyricBaseWidget() = default;

    std::vector<std::string> LyricBaseWidget::priorityLanguages() const {
        if (!m_languageContext)
            return {};
        return toStdVector(m_languageContext->priorityLanguages());
    }

    QString LyricBaseWidget::lyricText() const {
        return m_textEdit->toPlainText();
    }

    void LyricBaseWidget::setLyricText(const QString &text) {
        m_textEdit->setPlainText(text);
    }

    bool LyricBaseWidget::skipSlur() const {
        return m_skipSlur->isChecked();
    }

    void LyricBaseWidget::setSkipSlur(bool skip) {
        m_skipSlur->setChecked(skip);
    }

    int LyricBaseWidget::splitMode() const {
        return m_splitComboBox->currentIndex();
    }

    QString LyricBaseWidget::splitters() const {
        return m_splitters->text();
    }

    double LyricBaseWidget::fontSize() const {
        return m_textEdit->font().pointSizeF();
    }

    void LyricBaseWidget::setToTableVisible(bool visible) {
        m_btnToTable->setVisible(visible);
    }

    void LyricBaseWidget::setLyricPrevText(const QString &text) {
        m_btnLyricPrev->setText(text);
    }

    void LyricBaseWidget::onTextEditChanged() const {
        const QString text = this->m_textEdit->toPlainText();
        const auto splitRes = this->splitLyric(text);
        int count = 0;
        for (const auto &notes : splitRes) {
            count += static_cast<int>(notes.size());
        }
        this->m_textCountLabel->setText(
            QCoreApplication::translate("LyricBaseWidget", "Note Count: %L1").arg(count));
    }

    void LyricBaseWidget::onBtnImportLrcClicked() {
        const QString fileName = QFileDialog::getOpenFileName(
            this, QCoreApplication::translate("LyricBaseWidget", "Open Lrc File"), "",
            QCoreApplication::translate("LyricBaseWidget", "Lrc Files (*.lrc)"));
        if (fileName.isEmpty()) {
            return;
        }

        LrcTools::LrcDecoder decoder;
        if (!decoder.decode(fileName)) {
            QMessageBox::warning(
                this, QCoreApplication::translate("LyricBaseWidget", "Error"),
                QCoreApplication::translate("LyricBaseWidget", "Failed to decode lrc file."));
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
            splitNotes = LyricSplitter::splitAuto(lyric, priorityLanguages());
        } else if (splitType == ByChar) {
            splitNotes = LyricSplitter::splitByChar(lyric, priorityLanguages());
        } else if (splitType == Custom) {
            splitNotes = LyricSplitter::splitCustom(lyric, this->m_splitters->text().split(' '),
                                                    priorityLanguages());
        }

        return splitNotes;
    }

    void LyricBaseWidget::refreshLanguageCombo() {
        if (!m_languageComboBox || !m_languageContext)
            return;

        const QSignalBlocker blocker(m_languageComboBox);
        m_languageComboBox->clear();
        m_languageComboBox->addItem(QCoreApplication::translate("LyricBaseWidget", "Auto Detect"),
                                    QString());
        QSet<QString> added;
        for (const auto &language : m_languageContext->selectableLanguages()) {
            if (language.isEmpty() || added.contains(language))
                continue;
            added.insert(language);
            m_languageComboBox->addItem(LanguageDisplayUtils::displayName(language), language);
        }

        const auto choice = m_languageContext->choice();
        const auto index = m_languageComboBox->findData(choice);
        m_languageComboBox->setCurrentIndex(index >= 0 ? index : 0);
        m_previousLanguageChoice = choice;
        updateLanguageToolTip();
    }

    void LyricBaseWidget::revertLanguageChoice() {
        if (!m_languageContext)
            return;
        m_languageContext->setChoice(m_previousLanguageChoice);
        const auto index = m_languageComboBox->findData(m_previousLanguageChoice);
        {
            const QSignalBlocker blocker(m_languageComboBox);
            m_languageComboBox->setCurrentIndex(index >= 0 ? index : 0);
        }
        updateLanguageToolTip();
    }

    void LyricBaseWidget::updateLanguageToolTip() {
        if (!m_languageContext || !m_languageComboBox)
            return;
        m_languageComboBox->setToolTip(QCoreApplication::translate(
            "LyricBaseWidget", "The chosen language ranks first. Characters the lyric "
                               "engine cannot identify fall back to it."));
    }

    void LyricBaseWidget::onSplitComboBoxCurrentIndexChanged(int index) const {
        const auto splitType = static_cast<SplitType>(index);
        m_splitters->setVisible(splitType == Custom);
        this->onTextEditChanged();
        Q_EMIT modifyOption();
        Q_EMIT splitOptionChanged();
    }

} // namespace FillLyric
