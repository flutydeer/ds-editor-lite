#include "LyricTab.h"

#include <QFileDialog>

#include "UI/Controls/LineEdit.h"

namespace FillLyric {

    LyricTab::LyricTab(QList<Phonic *> phonics, QWidget *parent)
        : QWidget(parent), m_phonics(std::move(phonics)) {
        // textWidget
        m_lyricBaseWidget = new LyricBaseWidget();

        // lyricExtWidget
        m_lyricExtWidget = new LyricExtWidget(&notesCount);

        // lyric layout
        m_lyricLayout = new QHBoxLayout();
        m_lyricLayout->addWidget(m_lyricBaseWidget, 1);
        m_lyricLayout->addWidget(m_lyricExtWidget, 2);

        // main layout
        m_mainLayout = new QVBoxLayout(this);
        // 修改左右边距
        m_mainLayout->setContentsMargins(0, 10, 0, 10);
        m_mainLayout->addLayout(m_lyricLayout);

        connect(m_lyricBaseWidget->btnReReadNote, &QAbstractButton::clicked, this,
                &LyricTab::setPhonics);

        // phonicWidget signals
        connect(m_lyricExtWidget->m_btnInsertText, &QAbstractButton::clicked, this,
                &LyricTab::_on_btnInsertText_clicked);
        connect(m_lyricBaseWidget->m_btnToTable, &QAbstractButton::clicked, this,
                &LyricTab::_on_btnToTable_clicked);
        connect(m_lyricExtWidget->m_btnToText, &QAbstractButton::clicked, this,
                &LyricTab::_on_btnToText_clicked);

        // fold right
        connect(m_lyricBaseWidget->btnLyricPrev, &QPushButton::clicked, [this]() {
            m_lyricBaseWidget->btnLyricPrev->setText(
                m_lyricExtWidget->isVisible() ? tr("Lyric Prev") : tr("Fold Preview"));
            m_lyricExtWidget->setVisible(!m_lyricExtWidget->isVisible());
            m_lyricExtWidget->m_tableConfigWidget->setVisible(false);

            if (!m_lyricExtWidget->isVisible()) {
                Q_EMIT this->shrinkWindowRight(m_lyricBaseWidget->width() + 20);
            } else {
                Q_EMIT this->expandWindowRight();
            }
        });

        // fold left
        connect(m_lyricExtWidget->btnFoldLeft, &QPushButton::clicked, [this]() {
            m_lyricExtWidget->btnFoldLeft->setText(
                m_lyricBaseWidget->isVisible() ? tr("Expand Left") : tr("Fold Left"));
            m_lyricBaseWidget->setVisible(!m_lyricBaseWidget->isVisible());
        });
    }

    LyricTab::~LyricTab() = default;

    void LyricTab::setPhonics() {
        const bool skipSlurRes = m_lyricBaseWidget->skipSlur->isChecked();

        QStringList lyrics;
        QList<Phonic> phonics;
        for (const auto phonic : m_phonics) {
            if (skipSlurRes && (phonic->language == "Slur" || phonic->lyric == "-"))
                continue;
            phonics.append(*phonic);
            lyrics.append(phonic->lyric);
        }
        notesCount = static_cast<int>(phonics.size());
        m_lyricBaseWidget->m_textEdit->setText(lyrics.join(" "));
        m_lyricExtWidget->m_phonicWidget->_init(phonics);
    }

    QList<Phonic> LyricTab::exportPhonics() const {
        const auto model = m_lyricExtWidget->m_phonicWidget->model;
        model->expandFermata();

        const auto phonics =
            m_lyricExtWidget->isVisible()
                ? this->modelExport()
                : m_lyricBaseWidget->splitLyric(m_lyricBaseWidget->m_textEdit->toPlainText());
        return phonics;
    }

    bool LyricTab::exportSkipSlur() const {
        return m_lyricExtWidget->isVisible() ? m_lyricExtWidget->exportSkipSlur->isChecked()
                                             : m_lyricBaseWidget->skipSlur->isChecked();
    }

    QList<Phonic> LyricTab::modelExport() const {
        const auto model = m_lyricExtWidget->m_phonicWidget->model;
        const bool skipSpaceRes = m_lyricExtWidget->exportExcludeSpace;
        const bool skipSlurRes = exportSkipSlur();

        QList<Phonic> phonics;
        for (int i = 0; i < model->rowCount(); ++i) {
            const int col = skipSpaceRes ? model->currentLyricLength(i) : model->columnCount();
            for (int j = 0; j < col; ++j) {
                if (skipSlurRes &&
                    (model->cellLyricType(i, j) == "Slur" || model->cellLyric(i, j) == "-"))
                    continue;
                phonics.append(model->takeData(i, j));
            }
        }
        return phonics;
    }

    void LyricTab::_on_btnInsertText_clicked() const {
        const QString text =
            "Halloween蝉声--陪伴着qwe行云流浪---\n回-忆-开始132后安静遥望远方\n荒草覆没的古井--"
            "枯塘\n匀-散asdaw一缕过往\n";
        m_lyricBaseWidget->m_textEdit->setText(text);
    }

    void LyricTab::_on_btnToTable_clicked() const {
        const auto skipSlurRes = m_lyricBaseWidget->skipSlur->isChecked();
        const auto splitType =
            static_cast<SplitType>(m_lyricBaseWidget->m_splitComboBox->currentIndex());

        // 获取文本框的内容
        QString text = m_lyricBaseWidget->m_textEdit->toPlainText();
        if (skipSlurRes) {
            text = text.remove("-");
        }

        QList<Phonic> splitRes;
        if (splitType == Auto) {
            splitRes = CleanLyric::splitAuto(text);
        } else if (splitType == ByChar) {
            splitRes = CleanLyric::splitByChar(text);
        } else if (splitType == Custom) {
            splitRes =
                CleanLyric::splitCustom(text, m_lyricBaseWidget->m_splitters->text().split(' '));
        }

        m_lyricExtWidget->m_phonicWidget->_init(splitRes);
    }

    void LyricTab::_on_btnToText_clicked() const {
        const auto model = m_lyricExtWidget->m_phonicWidget->model;
        // 获取表格内容
        QStringList res;
        for (int i = 0; i < model->rowCount(); i++) {
            QStringList line;
            for (int j = 0; j < model->columnCount(); j++) {
                const auto lyric = model->cellLyric(i, j);
                if (!lyric.isEmpty()) {
                    line.append(lyric);
                }
            }
            res.append(line.join(""));
        }
        // 设置文本框内容
        m_lyricBaseWidget->m_textEdit->setText(res.join("\n"));
    }

} // FillLyric