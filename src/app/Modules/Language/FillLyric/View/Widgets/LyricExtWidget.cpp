#include "LyricExtWidget.h"

#include "../../History/ModelHistory.h"

#include "Model/AppOptions/AppOptions.h"

namespace FillLyric {
    LyricExtWidget::LyricExtWidget(int *notesCount, QWidget *parent)
        : QWidget(parent), notesCount(notesCount) {

        // phonicWidget
        m_phonicTableView = new PhonicTableView();
        m_wrapView = new LyricWrapView();

        // tableTop layout
        m_tableTopLayout = new QHBoxLayout();
        btnFoldLeft = new Button(tr("Fold Left"));
        btnToggleFermata = new Button(tr("Toggle Fermata"));
        autoWrapItem = new OptionsCardItem();
        autoWrapItem->setTitle(tr("Auto Wrap"));
        autoWrap = new SwitchButton();
        autoWrapItem->addWidget(autoWrap);

        btnUndo = new QPushButton();
        btnUndo->setEnabled(false);
        btnUndo->setMinimumSize(24, 24);
        btnUndo->setFixedWidth(24);
        btnUndo->setIcon(QIcon(":svg/icons/arrow_undo_16_filled_white.svg"));

        btnRedo = new QPushButton();
        btnRedo->setEnabled(false);
        btnRedo->setMinimumSize(24, 24);
        btnRedo->setFixedWidth(24);
        btnRedo->setIcon(QIcon(":svg/icons/arrow_redo_16_filled_white.svg"));

        btnTableConfig = new QPushButton();
        btnTableConfig->setFixedWidth(24);
        btnTableConfig->setIcon(QIcon(":svg/icons/settings_16_filled_white.svg"));

        m_btnInsertText = new Button(tr("Test"));
        m_tableTopLayout->addWidget(btnFoldLeft);
        m_tableTopLayout->addWidget(btnToggleFermata);
        m_tableTopLayout->addWidget(btnUndo);
        m_tableTopLayout->addWidget(btnRedo);
        m_tableTopLayout->addWidget(m_btnInsertText);
        m_tableTopLayout->addStretch(1);
        m_tableTopLayout->addWidget(autoWrapItem);
        m_tableTopLayout->addWidget(btnTableConfig);

        m_tableCountLayout = new QHBoxLayout();
        noteCountLabel = new QLabel("0/0");

        m_btnToText = new Button("<<");
        m_btnToText->setFixedSize(40, 20);
        m_tableCountLayout->addWidget(m_btnToText);
        m_tableCountLayout->addStretch(1);
        m_tableCountLayout->addWidget(noteCountLabel);

        // export option
        m_epOptLabelLayout = new QHBoxLayout();
        exportOptLabel = new QLabel(tr("Export Option:"));
        exportOptButton = new QPushButton();
        exportOptButton->setFixedSize(20, 20);
        exportOptButton->setIcon(QIcon(":/svg/icons/chevron_down_16_filled_white.svg"));

        m_epOptLabelLayout->addWidget(exportOptLabel);
        m_epOptLabelLayout->addStretch(1);
        m_epOptLabelLayout->addWidget(exportOptButton);

        // export option layout
        m_epOptWidget = new QWidget();
        m_epOptLayout = new QVBoxLayout();
        exportSkipSlur = new QCheckBox(tr("Skipping Slur"));
        exportSkipEndSpace = new QCheckBox(tr("Ignoring end of sentence spaces"));
        exportLanguage = new QCheckBox(tr("Automatically mark languages"));
        exportSkipEndSpace->setCheckState(Qt::Checked);

        m_epOptLayout->addWidget(exportSkipSlur);
        m_epOptLayout->addWidget(exportSkipEndSpace);
        m_epOptLayout->addWidget(exportLanguage);
        m_epOptLayout->addStretch(1);

        m_epOptWidget = new QWidget();
        m_epOptWidget->setVisible(false);
        m_epOptWidget->setContentsMargins(0, 0, 0, 0);
        m_epOptWidget->setLayout(m_epOptLayout);

        // table layout
        m_tableLayout = new QVBoxLayout();
        m_tableLayout->setContentsMargins(0, 0, 0, 0);
        m_tableLayout->addLayout(m_tableTopLayout);
        m_tableLayout->addWidget(m_wrapView);
        m_tableLayout->addLayout(m_tableCountLayout);
        m_tableLayout->addLayout(m_epOptLabelLayout);
        m_tableLayout->addWidget(m_epOptWidget);

        m_mainLayout = new QHBoxLayout();
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->addLayout(m_tableLayout);

        // table setting widget
        m_tableConfigWidget = new TableConfigWidget();
        m_tableConfigWidget->setVisible(false);

        m_mainLayout->addWidget(m_tableConfigWidget);
        this->setLayout(m_mainLayout);

        const auto appOptions = AppOptions::instance();
        QFont font = m_phonicTableView->font();
        font.setPointSize(appOptions->fillLyric()->tableFontSize);
        m_phonicTableView->setFont(font);
        autoWrap->setValue(appOptions->fillLyric()->autoWrap);
        m_phonicTableView->autoWrap = appOptions->fillLyric()->autoWrap;

        exportSkipSlur->setChecked(appOptions->fillLyric()->exportSkipSlur);
        exportSkipEndSpace->setChecked(appOptions->fillLyric()->exportSkipEndSpace);
        exportLanguage->setChecked(appOptions->fillLyric()->exportLanguage);

        m_phonicTableView->setColWidthRatio(appOptions->fillLyric()->tableColWidthRatio);
        m_phonicTableView->setRowHeightRatio(appOptions->fillLyric()->tableRowHeightRatio);
        m_phonicTableView->delegate->setFontSizeDiff(appOptions->fillLyric()->tableFontDiff);

        // undo redo
        m_history = m_wrapView->history();
        connect(btnUndo, &QPushButton::clicked, m_history, &QUndoStack::undo);
        connect(btnRedo, &QPushButton::clicked, m_history, &QUndoStack::redo);
        connect(m_history, &QUndoStack::canUndoChanged, btnUndo, &QPushButton::setEnabled);
        connect(m_history, &QUndoStack::canRedoChanged, btnRedo, &QPushButton::setEnabled);
        connect(autoWrap, &QCheckBox::clicked, m_history, &QUndoStack::clear);
        connect(m_phonicTableView, &PhonicTableView::historyReset, m_history, &QUndoStack::clear);

        connect(autoWrap, &QCheckBox::clicked, m_phonicTableView, &PhonicTableView::setAutoWrap);

        // phonicWidget toggleFermata
        connect(btnToggleFermata, &QPushButton::clicked, m_phonicTableView,
                &PhonicTableView::_on_btnToggleFermata_clicked);

        // phonicWidget label
        connect(m_phonicTableView->model, &PhonicModel::dataChanged, this,
                &LyricExtWidget::_on_modelDataChanged);

        // exportOptButton
        connect(exportOptButton, &QPushButton::clicked,
                [this]() { m_epOptWidget->setVisible(!m_epOptWidget->isVisible()); });

        // tableConfig
        connect(btnTableConfig, &QPushButton::clicked,
                [this]() { m_tableConfigWidget->setVisible(!m_tableConfigWidget->isVisible()); });

        // font size
        connect(m_phonicTableView, &PhonicTableView::wheelEventSignal, this,
                &LyricExtWidget::modifyOption);

        // tableConfigWidget
        connect(m_tableConfigWidget->m_colWidthRatioSpinBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](const double value) {
                    m_phonicTableView->setColWidthRatio(value);
                    modifyOption();
                });
        connect(m_tableConfigWidget->m_rowHeightSpinBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](const double value) {
                    m_phonicTableView->setRowHeightRatio(value);
                    modifyOption();
                });
        connect(m_tableConfigWidget->m_fontDiffSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                [=](const int value) {
                    m_phonicTableView->delegate->setFontSizeDiff(value);
                    modifyOption();
                });

        connect(autoWrap, &SwitchButton::clicked, this, &LyricExtWidget::modifyOption);
        connect(exportSkipSlur, &QCheckBox::stateChanged, this, &LyricExtWidget::modifyOption);
        connect(exportSkipEndSpace, &QCheckBox::stateChanged, this, &LyricExtWidget::modifyOption);
        connect(exportLanguage, &QCheckBox::stateChanged, this, &LyricExtWidget::modifyOption);
    }

    LyricExtWidget::~LyricExtWidget() = default;

    void LyricExtWidget::_on_modelDataChanged() const {
        const auto model = m_phonicTableView->model;
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

    void LyricExtWidget::modifyOption() const {
        const auto options = AppOptions::instance()->fillLyric();
        options->tableFontSize = m_phonicTableView->font().pointSize();

        options->tableColWidthRatio = m_tableConfigWidget->m_colWidthRatioSpinBox->value();
        options->tableRowHeightRatio = m_tableConfigWidget->m_rowHeightSpinBox->value();
        options->tableFontDiff = m_tableConfigWidget->m_fontDiffSpinBox->value();

        options->autoWrap = autoWrap->value();
        options->exportSkipSlur = exportSkipSlur->isChecked();
        options->exportSkipEndSpace = exportSkipEndSpace->isChecked();
        options->exportLanguage = exportLanguage->isChecked();
        AppOptions::instance()->saveAndNotify();
    }

} // FillLyric