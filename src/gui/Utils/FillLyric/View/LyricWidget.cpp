#include "LyricWidget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardItemModel>
#include <utility>

#include "../Utils/LrcTools/LrcDecoder.h"

namespace FillLyric {

    LyricWidget::LyricWidget(QList<PhonicNote *> phonicNotes, QWidget *parent)
        : QWidget(parent), m_phonicNotes(std::move(phonicNotes)) {

        // textEdit
        m_textEdit = new PhonicTextEdit();
        m_textEdit->setPlaceholderText("请输入歌词");

        // phonicWidget
        m_phonicWidget = new PhonicWidget(m_phonicNotes);

        // top layout
        m_topLayout = new QHBoxLayout();
        m_textCountLabel = new QLabel("字符数: 0");
        btnUndo = new QPushButton("撤销");
        btnRedo = new QPushButton("重做");
        noteCountLabel = new QLabel("0/0");
        m_topLayout->addWidget(m_textCountLabel);
        m_topLayout->addStretch(1);
        m_topLayout->addWidget(btnUndo);
        m_topLayout->addWidget(btnRedo);
        m_topLayout->addWidget(noteCountLabel);

        // lyric option layout
        m_lyricOptLayout = new QVBoxLayout();
        btnInsertText = new QPushButton("插入测试文本");
        btnToTable = new QPushButton(">>");
        btnToText = new QPushButton("<<");
        btnImportLrc = new QPushButton("导入lrc");
        btnToggleFermata = new QPushButton("收放延音符");

        m_lyricOptLayout->addStretch(1);
        m_lyricOptLayout->addWidget(btnInsertText);
        m_lyricOptLayout->addWidget(btnToTable);
        m_lyricOptLayout->addWidget(btnToText);
        m_lyricOptLayout->addWidget(btnToggleFermata);
        m_lyricOptLayout->addWidget(btnImportLrc);
        m_lyricOptLayout->addStretch(1);

        // lyric layout
        m_lyricLayout = new QHBoxLayout();
        m_lyricLayout->addWidget(m_textEdit);
        m_lyricLayout->addLayout(m_lyricOptLayout);
        m_lyricLayout->addWidget(m_phonicWidget->tableView);

        // bottom layout
        m_bottomLayout = new QHBoxLayout();
        btnSplitGroup = new QButtonGroup();
        btnSplitAuto = new QRadioButton("Auto");
        btnSplitByChar = new QRadioButton("ByChar");
        btnSplitCustom = new QRadioButton("Custom");
        btnSplitByReg = new QRadioButton("ByReg");

        btnSplitGroup->addButton(btnSplitAuto, 0);
        btnSplitGroup->addButton(btnSplitByChar, 1);
        btnSplitGroup->addButton(btnSplitCustom, 2);
        btnSplitGroup->addButton(btnSplitByReg, 3);
        btnSplitAuto->setChecked(true);

        m_bottomLayout->addWidget(btnSplitAuto);
        m_bottomLayout->addWidget(btnSplitByChar);
        m_bottomLayout->addWidget(btnSplitCustom);
        m_bottomLayout->addWidget(btnSplitByReg);
        m_bottomLayout->addStretch(1);

        // main layout
        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->addLayout(m_topLayout);
        m_mainLayout->addLayout(m_lyricLayout);
        m_mainLayout->addLayout(m_bottomLayout);

        // phonicWidget signals
        connect(btnInsertText, &QAbstractButton::clicked, this,
                &LyricWidget::_on_btnInsertText_clicked);
        connect(btnToTable, &QAbstractButton::clicked, this, &LyricWidget::_on_btnToTable_clicked);
        connect(btnToText, &QAbstractButton::clicked, this, &LyricWidget::_on_btnToText_clicked);
        connect(btnImportLrc, &QAbstractButton::clicked, this,
                &LyricWidget::_on_btnImportLrc_clicked);

        // phonicWidget toggleFermata
        connect(btnToggleFermata, &QPushButton::clicked, m_phonicWidget,
                &PhonicWidget::_on_btnToggleFermata_clicked);

        // phonicWidget label
        connect(m_phonicWidget->model, &PhonicModel::dataChanged, this,
                &LyricWidget::_on_modelDataChanged);

        // textEdit label
        connect(m_textEdit, &PhonicTextEdit::textChanged, this, &LyricWidget::_on_textEditChanged);

        // undo redo
        auto modelHistory = ModelHistory::instance();
        connect(btnUndo, &QPushButton::clicked, modelHistory, &ModelHistory::undo);
        connect(btnRedo, &QPushButton::clicked, modelHistory, &ModelHistory::redo);
    }

    LyricWidget::~LyricWidget() = default;

    void LyricWidget::_on_textEditChanged() {
        // 获取文本框的内容
        QString text = m_textEdit->toPlainText();
        // 获取歌词
        auto res = CleanLyric::cleanLyric(text).first;
        int lyricCount = 0;
        for (auto &line : res) {
            lyricCount += (int) line.size();
        }
        m_textCountLabel->setText(QString("字符数: %1").arg(lyricCount));
    }

    void LyricWidget::_on_modelDataChanged() {
        auto model = m_phonicWidget->model;
        int lyricCount = 0;
        for (int i = 0; i < model->rowCount(); i++) {
            for (int j = 0; j < model->columnCount(); j++) {
                auto lyric = model->data(model->index(i, j), Qt::DisplayRole).toString();
                if (!lyric.isEmpty()) {
                    lyricCount++;
                }
                auto fermataCount = (int) model->cellFermata(i, j).size();
                if (fermataCount > 0) {
                    lyricCount += fermataCount;
                }
            }
        }
        noteCountLabel->setText(QString::number(lyricCount) + "/" + QString::number(notesCount));
    }


    void LyricWidget::_on_btnInsertText_clicked() {
        // 测试文本
        QString text = "蝉声--陪伴着行云流浪---\n回-忆-开始后安静遥望远方\n荒草覆没的古井--"
                       "枯塘\n匀-散一缕过往\n";
        m_textEdit->setText(text);
    }

    void LyricWidget::_on_btnToTable_clicked() {
        // 获取文本框的内容
        QString text = m_textEdit->toPlainText();
        m_phonicWidget->_init(CleanLyric::cleanLyric(text).first);
    }

    void LyricWidget::_on_btnToText_clicked() {
        auto model = m_phonicWidget->model;
        // 获取表格内容
        QStringList res;
        for (int i = 0; i < model->rowCount(); i++) {
            QStringList line;
            for (int j = 0; j < model->columnCount(); j++) {
                auto lyric = model->data(model->index(i, j), Qt::DisplayRole).toString();
                if (!lyric.isEmpty()) {
                    line.append(lyric);
                }
            }
            res.append(line.join(""));
        }
        // 设置文本框内容
        m_textEdit->setText(res.join("\n"));
    }

    void LyricWidget::_on_btnImportLrc_clicked() {
        // 打开文件对话框
        QString fileName =
            QFileDialog::getOpenFileName(this, "打开歌词文件", "", "歌词文件(*.lrc)");
        if (fileName.isEmpty()) {
            return;
        }
        // 创建LrcDecoder对象
        LrcTools::LrcDecoder decoder;
        // 解析歌词文件
        auto res = decoder.decode(fileName);
        if (!res) {
            // 解析失败
            QMessageBox::warning(this, "错误", "解析lrc文件失败");
            return;
        }
        // 获取歌词文件的元数据
        auto metadata = decoder.dumpMetadata();
        qDebug() << "metadata: " << metadata;

        // 获取歌词文件的歌词
        auto lyrics = decoder.dumpLyrics();
        // 设置文本框内容
        m_textEdit->setText(lyrics.join("\n"));
    }

} // FillLyric