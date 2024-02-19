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
        setAttribute(Qt::WA_StyledBackground);
        setObjectName("LyricWidget");

        // textEdit top
        m_textTopLayout = new QHBoxLayout();
        btnImportLrc = new QPushButton("导入lrc");
        m_textCountLabel = new QLabel("字符数: 0");
        m_textTopLayout->addWidget(btnImportLrc);
        m_textTopLayout->addStretch(1);
        m_textTopLayout->addWidget(m_textCountLabel);

        // textEdit
        m_textEdit = new PhonicTextEdit();
        m_textEdit->setPlaceholderText("请输入歌词");

        m_textEditLayout = new QVBoxLayout();
        m_textEditLayout->addLayout(m_textTopLayout);
        m_textEditLayout->addWidget(m_textEdit);

        // phonicWidget
        m_phonicWidget = new PhonicWidget(m_phonicNotes);

        // tableTop layout
        m_tableTopLayout = new QHBoxLayout();
        btnToggleFermata = new QPushButton("收放延音符");
        btnUndo = new QPushButton("撤销");
        btnRedo = new QPushButton("重做");
        noteCountLabel = new QLabel("0/0");

        m_tableTopLayout->addWidget(btnToggleFermata);
        m_tableTopLayout->addWidget(btnUndo);
        m_tableTopLayout->addWidget(btnRedo);
        m_tableTopLayout->addStretch(1);
        m_tableTopLayout->addWidget(noteCountLabel);

        // lyric option layout
        m_lyricOptLayout = new QVBoxLayout();
        btnInsertText = new QPushButton("插入测试文本");
        btnToTable = new QPushButton(">>");
        btnToText = new QPushButton("<<");

        m_lyricOptLayout->addStretch(1);
        m_lyricOptLayout->addWidget(btnInsertText);
        m_lyricOptLayout->addWidget(btnToTable);
        m_lyricOptLayout->addWidget(btnToText);
        m_lyricOptLayout->addStretch(1);

        // table layout
        m_tableLayout = new QVBoxLayout();
        m_tableLayout->addLayout(m_tableTopLayout);
        m_tableLayout->addWidget(m_phonicWidget->tableView);

        // lyric layout
        m_lyricLayout = new QHBoxLayout();
        m_lyricLayout->addLayout(m_textEditLayout, 2);
        m_lyricLayout->addLayout(m_lyricOptLayout);
        m_lyricLayout->addLayout(m_tableLayout, 3);

        skipSlur = new QCheckBox("skip slur");
        splitBySpace = new QCheckBox("split by space");
        m_skipSlurLayout = new QHBoxLayout();
        m_skipSlurLayout->addWidget(skipSlur);
        m_skipSlurLayout->addStretch(1);
        m_skipSlurLayout->addWidget(splitBySpace);

        // bottom layout
        m_splitLayout = new QHBoxLayout();
        splitLabel = new QLabel("Split Mode:");
        splitComboBox = new QComboBox();
        splitComboBox->addItem("Auto");
        splitComboBox->addItem("By Char");
        splitComboBox->addItem("Custom");
        splitComboBox->addItem("By Reg");
        btnSetting = new QPushButton("Setting");

        m_splitLayout->addWidget(splitLabel);
        m_splitLayout->addWidget(splitComboBox);
        m_splitLayout->addStretch(1);
        m_splitLayout->addWidget(btnSetting);

        m_textEditLayout->addLayout(m_skipSlurLayout);
        m_textEditLayout->addLayout(m_splitLayout);

        // main layout
        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->addLayout(m_lyricLayout);

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