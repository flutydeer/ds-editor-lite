#include "LyricBaseWidget.h"

#include <QFileDialog>
#include <QMessageBox>

#include "../../../Utils/CleanLyric.h"

#include "../../../Utils/LrcTools/LrcDecoder.h"

namespace FillLyric {
    LyricBaseWidget::LyricBaseWidget(QWidget *parent) : QWidget(parent) {
        // textEdit top
        m_textTopLayout = new QHBoxLayout();
        btnImportLrc = new Button("导入lrc");
        btnReReadNote = new Button("重读音符");
        btnLyricPrev = new Button("折叠预览");
        m_textTopLayout->addWidget(btnImportLrc);
        m_textTopLayout->addWidget(btnReReadNote);
        m_textTopLayout->addStretch(1);
        m_textTopLayout->addWidget(btnLyricPrev);

        // textEdit
        m_textEdit = new PhonicTextEdit();
        m_textEdit->setPlaceholderText("请输入歌词");

        m_textBottomLayout = new QHBoxLayout();
        m_textCountLabel = new QLabel("字符数: 0");
        m_textBottomLayout->addStretch(1);
        m_textBottomLayout->addWidget(m_textCountLabel);

        m_textEditLayout = new QVBoxLayout();
        m_textEditLayout->setContentsMargins(0, 0, 0, 0);
        m_textEditLayout->addLayout(m_textTopLayout);
        m_textEditLayout->addWidget(m_textEdit);
        m_textEditLayout->addLayout(m_textBottomLayout);

        // bottom layout
        m_splitLayout = new QHBoxLayout();
        m_splitLabel = new QLabel("Split Mode :");
        m_splitComboBox = new ComboBox(true);
        m_splitComboBox->addItems({"Auto", "By Char", "Custom", "By Reg"});
        m_splitLayout->addWidget(m_splitLabel);
        m_splitLayout->addWidget(m_splitComboBox);
        m_textEditLayout->addLayout(m_splitLayout);

        skipSlur = new QCheckBox("Skip Slur Note");
        excludeSpace = new QCheckBox("Exclude Space");
        excludeSpace->setCheckState(Qt::Checked);
        m_skipSlurLayout = new QHBoxLayout();
        m_skipSlurLayout->addWidget(skipSlur);
        m_skipSlurLayout->addStretch(1);
        m_skipSlurLayout->addWidget(excludeSpace);

        m_textEditLayout->addLayout(m_skipSlurLayout);

        this->setLayout(m_textEditLayout);

        // textEditTop signals
        connect(btnImportLrc, &QAbstractButton::clicked, this,
                &LyricBaseWidget::_on_btnImportLrc_clicked);

        // textEdit label
        connect(m_textEdit, &PhonicTextEdit::textChanged, this,
                &LyricBaseWidget::_on_textEditChanged);
    }

    LyricBaseWidget::~LyricBaseWidget() = default;

    void LyricBaseWidget::_on_textEditChanged() const {
        // 获取文本框的内容
        const QString text = this->m_textEdit->toPlainText();
        // 获取歌词
        QList<Phonic> res = this->splitLyric(text);
        this->m_textCountLabel->setText(QString("字符数: %1").arg(res.size()));
    }

    void LyricBaseWidget::_on_btnImportLrc_clicked() {
        // 打开文件对话框
        const QString fileName =
            QFileDialog::getOpenFileName(this, "打开歌词文件", "", "歌词文件(*.lrc)");
        if (fileName.isEmpty()) {
            return;
        }
        // 创建LrcDecoder对象
        LrcTools::LrcDecoder decoder;
        // 解析歌词文件
        if (!decoder.decode(fileName)) {
            // 解析失败
            QMessageBox::warning(this, "错误", "解析lrc文件失败");
            return;
        }
        // 获取歌词文件的元数据
        const auto metadata = decoder.dumpMetadata();
        qDebug() << "metadata: " << metadata;

        // 获取歌词文件的歌词
        const auto lyrics = decoder.dumpLyrics();
        // 设置文本框内容
        this->m_textEdit->setText(lyrics.join("\n"));
    }

    QList<Phonic> LyricBaseWidget::splitLyric(const QString &lyric) const {
        const bool skipSlurRes = this->skipSlur->isChecked();
        const auto splitType = static_cast<SplitType>(this->m_splitComboBox->currentIndex());
        QList<Phonic> splitPhonics;
        if (splitType == SplitType::Auto) {
            splitPhonics = CleanLyric::splitAuto(lyric, this->excludeSpace->isChecked());
        } else if (splitType == SplitType::ByChar) {
            splitPhonics = CleanLyric::splitByChar(lyric, this->excludeSpace->isChecked());
        } else if (splitType == SplitType::Custom) {
            splitPhonics = CleanLyric::splitCustom(lyric, QStringList() << "-",
                                                   this->excludeSpace->isChecked());
        } else {
        }

        QList<Phonic> skipSlurPhonics;
        if (skipSlurRes) {
            for (const auto &phonic : splitPhonics) {
                if (phonic.lyricType != Slur && phonic.lyric != "-") {
                    skipSlurPhonics.append(phonic);
                }
            }
        }

        const auto res = skipSlurRes ? skipSlurPhonics : splitPhonics;
        return res;
    }

} // FillLyric