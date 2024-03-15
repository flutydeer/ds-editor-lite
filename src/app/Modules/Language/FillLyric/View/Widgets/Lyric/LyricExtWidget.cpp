#include "LyricExtWidget.h"

#include "../../../History/ModelHistory.h"

namespace FillLyric {
    LyricExtWidget::LyricExtWidget(int *notesCount, QWidget *parent)
        : QWidget(parent), notesCount(notesCount) {

        // phonicWidget
        m_phonicWidget = new PhonicWidget();

        // tableTop layout
        m_tableTopLayout = new QHBoxLayout();
        btnFoldLeft = new Button(tr("Fold Left"));
        btnToggleFermata = new Button(tr("Toggle Fermata"));
        autoWrap = new QCheckBox(tr("Auto Wrap"));
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
        exportLabel = new QLabel(tr("Export Option:"));
        exportSkipSlur = new QCheckBox(tr("Skipping Slur"));
        exportExcludeSpace = new QCheckBox(tr("Ignoring end of sentence spaces"));
        exportLanguage = new QCheckBox(tr("Automatically mark languages"));
        exportExcludeSpace->setCheckState(Qt::Checked);

        m_tableBottomLayout->addWidget(exportLabel);
        m_tableBottomLayout->addWidget(exportSkipSlur);
        m_tableBottomLayout->addWidget(exportExcludeSpace);
        m_tableBottomLayout->addWidget(exportLanguage);
        m_tableBottomLayout->addStretch(1);

        // table layout
        m_tableLayout = new QVBoxLayout();
        m_tableLayout->setContentsMargins(0, 0, 0, 0);
        m_tableLayout->addLayout(m_tableTopLayout);
        m_tableLayout->addWidget(m_phonicWidget->tableView);
        m_tableLayout->addLayout(m_tableCountLayout);
        m_tableLayout->addLayout(m_tableBottomLayout);

        m_mainLayout = new QHBoxLayout();
        m_mainLayout->addLayout(m_tableLayout);

        // table setting widget
        m_tableConfigWidget = new TableConfigWidget(m_phonicWidget->tableView);
        m_tableConfigWidget->setVisible(false);

        m_mainLayout->addWidget(m_tableConfigWidget);
        this->setLayout(m_mainLayout);

        // undo redo
        const auto modelHistory = ModelHistory::instance();
        connect(btnUndo, &QPushButton::clicked, modelHistory, &ModelHistory::undo);
        connect(btnRedo, &QPushButton::clicked, modelHistory, &ModelHistory::redo);
        connect(modelHistory, &ModelHistory::undoRedoChanged, this,
                [=](const bool canUndo, const bool canRedo) {
                    btnUndo->setEnabled(canUndo);
                    btnRedo->setEnabled(canRedo);
                });
        connect(autoWrap, &QCheckBox::stateChanged, modelHistory, &ModelHistory::reset);
        connect(m_phonicWidget, &PhonicWidget::historyReset, modelHistory, &ModelHistory::reset);

        connect(autoWrap, &QCheckBox::stateChanged, m_phonicWidget, &PhonicWidget::setAutoWrap);

        // phonicWidget toggleFermata
        connect(btnToggleFermata, &QPushButton::clicked, m_phonicWidget,
                &PhonicWidget::_on_btnToggleFermata_clicked);

        // phonicWidget label
        connect(m_phonicWidget->model, &PhonicModel::dataChanged, this,
                &LyricExtWidget::_on_modelDataChanged);

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

    LyricExtWidget::~LyricExtWidget() = default;

    void LyricExtWidget::_on_modelDataChanged() const {
        const auto model = m_phonicWidget->model;
        int lyricCount = 0;
        for (int i = 0; i < model->rowCount(); i++) {
            for (int j = 0; j < model->columnCount(); j++) {
                if (!model->cellLyric(i, j).isEmpty()) {
                    lyricCount++;
                }

                if (const int fermataCount =
                        static_cast<int>(model->cellFermata(i, j).size()) > 0) {
                    lyricCount += fermataCount;
                }
            }
        }
        noteCountLabel->setText(QString::number(lyricCount) + "/" + QString::number(*notesCount));
    }

} // FillLyric