#include "LyricWidget.h"

#include <QFileDialog>
#include <QMessageBox>

#include "../Utils/LrcTools/LrcDecoder.h"
#include "Controls/LineEdit.h"

namespace FillLyric {

    LyricWidget::LyricWidget(QList<Phonic *> phonics, QWidget *parent)
        : QWidget(parent), m_phonics(std::move(phonics)) {
        setStyleSheet(
            "QPushButton {border: none; background: none; max-width: 100px; padding: 4px;}"
            "QPushButton:hover { background: #1AFFFFFF; }"
            "QPushButton:pressed { background: #10FFFFFF; }");
        // textWidget
        m_textEditWidget = new QWidget();

        // textEdit top
        m_textTopLayout = new QHBoxLayout();
        btnImportLrc = new Button("导入lrc");
        btnLyricPrev = new Button("折叠预览");
        m_textTopLayout->addWidget(btnImportLrc);
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

        skipSlur = new QCheckBox("Skip Slur Note");
        excludeSpace = new QCheckBox("Exclude Space");
        excludeSpace->setCheckState(Qt::Checked);
        m_skipSlurLayout = new QHBoxLayout();
        m_skipSlurLayout->addWidget(skipSlur);
        m_skipSlurLayout->addStretch(1);
        m_skipSlurLayout->addWidget(excludeSpace);

        m_textEditLayout->addLayout(m_skipSlurLayout);

        m_textEditWidget->setLayout(m_textEditLayout);


        // lyric option widget
        m_lyricOptWidget = new QWidget();

        // lyric option layout
        m_lyricOptLayout = new QVBoxLayout();
        m_lyricOptLayout->setContentsMargins(6, 0, 6, 0);
        btnInsertText = new Button("测试");
        btnToTable = new Button(">>");
        btnToText = new Button("<<");

        m_lyricOptLayout->addStretch(1);
        m_lyricOptLayout->addWidget(btnInsertText);
        m_lyricOptLayout->addWidget(btnToTable);
        m_lyricOptLayout->addWidget(btnToText);
        m_lyricOptLayout->addStretch(1);

        // bottom layout
        splitLabel = new QLabel("Split Mode :");
        splitComboBox = new ComboBox(true);
        splitComboBox->addItems({"Auto", "By Char", "Custom", "By Reg"});
        btnRegSetting = new Button("Setting");
        m_splitters = new LineEdit();
        m_splitters->setMaximumWidth(85);
        m_splitters->setToolTip("Custom delimiter, input with space intervals. If you want to use "
                                "spaces as separators, please check the checkbox above.");

        m_splitters->setVisible(false);
        btnRegSetting->setVisible(false);

        m_lyricOptLayout->addWidget(splitLabel);
        m_lyricOptLayout->addWidget(splitComboBox);
        m_lyricOptLayout->addWidget(btnRegSetting);
        m_lyricOptLayout->addWidget(m_splitters);
        m_lyricOptLayout->addStretch(1);

        m_lyricOptWidget->setLayout(m_lyricOptLayout);

        // tableWidget
        m_tableWidget = new QWidget();

        // phonicWidget
        m_phonicWidget = new PhonicWidget();

        // tableTop layout
        m_tableTopLayout = new QHBoxLayout();
        btnFoldLeft = new Button("收起左侧");
        btnToggleFermata = new Button("收放延音符");
        autoWrap = new QCheckBox("Auto Wrap");
        btnUndo = new QPushButton();

        btnUndo->setMinimumSize(24, 24);
        btnUndo->setFixedWidth(24);
        btnUndo->setIcon(QIcon(":svg/icons/arrow_undo_16_filled_white.svg"));
        btnRedo = new QPushButton();

        btnRedo->setMinimumSize(24, 24);
        btnRedo->setFixedWidth(24);
        btnRedo->setIcon(QIcon(":svg/icons/arrow_redo_16_filled_white.svg"));
        btnTableConfig = new QPushButton();
        btnTableConfig->setFixedWidth(24);
        btnTableConfig->setIcon(QIcon(":svg/icons/settings_16_filled_white.svg"));

        m_tableTopLayout->addWidget(btnFoldLeft);
        m_tableTopLayout->addWidget(btnToggleFermata);
        m_tableTopLayout->addWidget(btnUndo);
        m_tableTopLayout->addWidget(btnRedo);
        m_tableTopLayout->addStretch(1);
        m_tableTopLayout->addWidget(autoWrap);
        m_tableTopLayout->addWidget(btnTableConfig);

        m_tableCountLayout = new QHBoxLayout();
        noteCountLabel = new QLabel("0/0");
        m_tableCountLayout->addStretch(1);
        m_tableCountLayout->addWidget(noteCountLabel);

        m_tableBottomLayout = new QHBoxLayout();
        exportLabel = new QLabel("Export Config :");
        exportSkipSlur = new QCheckBox("Skip Slur");
        exportExcludeSpace = new QCheckBox("Ignoring end of sentence spaces");
        exportExcludeSpace->setCheckState(Qt::Checked);

        m_tableBottomLayout->addWidget(exportLabel);
        m_tableBottomLayout->addWidget(exportSkipSlur);
        m_tableBottomLayout->addWidget(exportExcludeSpace);
        m_tableBottomLayout->addStretch(1);

        // table layout
        m_tableLayout = new QVBoxLayout();
        m_tableLayout->setContentsMargins(0, 0, 0, 0);
        m_tableLayout->addLayout(m_tableTopLayout);
        m_tableLayout->addWidget(m_phonicWidget->tableView);
        m_tableLayout->addLayout(m_tableCountLayout);
        m_tableLayout->addLayout(m_tableBottomLayout);

        m_tableWidget->setLayout(m_tableLayout);

        // table setting widget
        m_tableConfigWidget = new TableConfigWidget(m_phonicWidget->tableView);
        m_tableConfigWidget->setVisible(false);

        // lyric layout
        m_lyricLayout = new QHBoxLayout();
        m_lyricLayout->addWidget(m_textEditWidget, 3);
        m_lyricLayout->addWidget(m_lyricOptWidget);
        m_lyricLayout->addWidget(m_tableWidget, 5);
        m_lyricLayout->addWidget(m_tableConfigWidget);

        // main layout
        m_mainLayout = new QVBoxLayout(this);
        // 修改左右边距
        m_mainLayout->setContentsMargins(0, 10, 0, 10);
        m_mainLayout->addLayout(m_lyricLayout);

        // phonicWidget signals
        connect(btnInsertText, &QAbstractButton::clicked, this,
                &LyricWidget::_on_btnInsertText_clicked);
        connect(btnToTable, &QAbstractButton::clicked, this, &LyricWidget::_on_btnToTable_clicked);
        connect(btnToText, &QAbstractButton::clicked, this, &LyricWidget::_on_btnToText_clicked);
        connect(btnImportLrc, &QAbstractButton::clicked, this,
                &LyricWidget::_on_btnImportLrc_clicked);

        connect(autoWrap, &QCheckBox::stateChanged, m_phonicWidget, &PhonicWidget::setAutoWrap);

        // phonicWidget toggleFermata
        connect(btnToggleFermata, &QPushButton::clicked, m_phonicWidget,
                &PhonicWidget::_on_btnToggleFermata_clicked);

        // phonicWidget label
        connect(m_phonicWidget->model, &PhonicModel::dataChanged, this,
                &LyricWidget::_on_modelDataChanged);

        // textEdit label
        connect(m_textEdit, &PhonicTextEdit::textChanged, this, &LyricWidget::_on_textEditChanged);

        // fold right
        connect(btnLyricPrev, &QPushButton::clicked, [this]() {
            btnLyricPrev->setText(m_tableWidget->isVisible() ? "预览歌词" : "折叠预览");
            m_lyricOptWidget->setVisible(!m_lyricOptWidget->isVisible());
            m_tableWidget->setVisible(!m_tableWidget->isVisible());
            m_tableConfigWidget->setVisible(false);

            if (!m_tableWidget->isVisible()) {
                int width = m_textEdit->width();
                Q_EMIT this->shrinkWindowRight(width + 20);
            } else {
                Q_EMIT this->expandWindowRight();
            }
        });

        // fold left
        connect(btnFoldLeft, &QPushButton::clicked, [this]() {
            btnFoldLeft->setText(m_textEditWidget->isVisible() ? "展开左侧" : "收起左侧");
            m_textEditWidget->setVisible(!m_textEditWidget->isVisible());
            m_lyricOptWidget->setVisible(!m_lyricOptWidget->isVisible());
        });

        // undo redo
        auto modelHistory = ModelHistory::instance();
        connect(btnUndo, &QPushButton::clicked, modelHistory, &ModelHistory::undo);
        connect(btnRedo, &QPushButton::clicked, modelHistory, &ModelHistory::redo);
        connect(modelHistory, &ModelHistory::undoRedoChanged, this,
                [=](bool canUndo, bool canRedo) {
                    btnUndo->setEnabled(canUndo);
                    btnRedo->setEnabled(canRedo);
                });
        connect(autoWrap, &QCheckBox::stateChanged, modelHistory, &ModelHistory::reset);
        connect(m_phonicWidget, &PhonicWidget::historyReset, modelHistory, &ModelHistory::reset);

        // splitComboBox
        connect(splitComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &LyricWidget::_on_splitComboBox_currentIndexChanged);

        // tableConfig
        connect(btnTableConfig, &QPushButton::clicked,
                [this]() { m_tableConfigWidget->setVisible(!m_tableConfigWidget->isVisible()); });

        // tableConfigWidget
        connect(m_tableConfigWidget->m_colWidthRatioSpinBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged), m_phonicWidget,
                &PhonicWidget::setColWidthRatio);
        connect(m_tableConfigWidget->m_rowHeightSpinBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged), m_phonicWidget,
                &PhonicWidget::setRowHeightRatio);
        connect(m_tableConfigWidget->m_fontDiffSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                m_phonicWidget->delegate, &PhonicDelegate::setFontSizeDiff);
    }

    LyricWidget::~LyricWidget() = default;

    void LyricWidget::setPhonics() {
        bool skipSlurRes = skipSlur->isChecked();

        QList<Phonic> phonics;
        for (auto phonic : m_phonics) {
            if (skipSlurRes && (phonic->lyricType == TextType::Slur || phonic->lyric == "-"))
                continue;
            phonics.append(*phonic);
        }
    }

    QList<Phonic> LyricWidget::exportPhonics() {
        bool skipSpaceRes = excludeSpace->isChecked();
        bool skipSlurRes = exportSkipSlur->isChecked();

        auto model = m_phonicWidget->model;
        model->expandFermata();


        QList<Phonic> phonics;
        for (int i = 0; i < model->rowCount(); ++i) {
            int col = skipSpaceRes ? model->currentLyricLength(i) : model->columnCount();
            for (int j = 0; j < col; ++j) {
                if (skipSlurRes && model->cellLyricType(i, j) == TextType::Slur)
                    continue;
                phonics.append(model->takeData(i, j));
            }
        }
        return phonics;
    }

    void LyricWidget::_on_textEditChanged() {
        auto splitType = SplitType(this->splitComboBox->currentIndex());
        // 获取文本框的内容
        QString text = m_textEdit->toPlainText();
        // 获取歌词
        QList<Phonic> res;
        if (splitType == SplitType::Auto) {
            res = CleanLyric::splitAuto(text, excludeSpace->isChecked());
        } else if (splitType == SplitType::ByChar) {
            res = CleanLyric::splitByChar(text, excludeSpace->isChecked());
        } else if (splitType == SplitType::Custom) {
            res = CleanLyric::splitCustom(text, QStringList() << "-", excludeSpace->isChecked());
        } else {
        }
        m_textCountLabel->setText(QString("字符数: %1").arg(res.size()));
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
        QString text =
            "Halloween蝉声--陪伴着qwe行云流浪---\n回-忆-开始132后安静遥望远方\n荒草覆没的古井--"
            "枯塘\n匀-散asdaw一缕过往\n";
        m_textEdit->setText(text);
    }

    void LyricWidget::_on_btnToTable_clicked() {
        auto skipSlurRes = this->skipSlur->isChecked();
        auto excludeSpaceRes = this->excludeSpace->isChecked();
        auto splitType = SplitType(this->splitComboBox->currentIndex());

        // 获取文本框的内容
        QString text = m_textEdit->toPlainText();
        if (skipSlurRes) {
            text = text.remove("-");
        }

        QList<Phonic> splitRes;
        if (splitType == SplitType::Auto) {
            splitRes = CleanLyric::splitAuto(text, excludeSpaceRes);
        } else if (splitType == SplitType::ByChar) {
            splitRes = CleanLyric::splitByChar(text, excludeSpaceRes);
        } else if (splitType == SplitType::Custom) {
            splitRes =
                CleanLyric::splitCustom(text, m_splitters->text().split(' '), excludeSpaceRes);
        } else {
        }

        m_phonicWidget->_init(splitRes);
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

    void LyricWidget::_on_splitComboBox_currentIndexChanged(int index) {
        auto splitType = SplitType(index);
        QString checkBoxName = "Exclude Space";
        if (splitType == SplitType::Custom)
            checkBoxName = "Split By Space";

        m_splitters->setVisible(splitType == SplitType::Custom);
        btnRegSetting->setVisible(splitType == SplitType::ByReg);

        excludeSpace->setText(checkBoxName);
    }

} // FillLyric