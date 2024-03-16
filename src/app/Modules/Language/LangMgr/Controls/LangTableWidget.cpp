#include "LangTableWidget.h"

#include <QDragEnterEvent>
#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>

#include "../ILanguageManager.h"

namespace LangMgr {
    LangTableWidget::LangTableWidget(QWidget *parent) : QTableWidget(parent) {
        const auto langMgr = ILanguageManager::instance();

        this->setDragDropMode(InternalMove);
        this->setDropIndicatorShown(true);
        this->setSelectionBehavior(SelectRows);


        this->setColumnCount(4);
        this->setHorizontalHeaderLabels(QStringList() << tr("Language") << tr("Discard Result")
                                                      << tr("Author") << tr("Description"));

        const auto langConfigs = langMgr->languageConfig();
        for (const auto &config : langConfigs) {
            const auto row = this->rowCount();
            this->insertRow(row);
            this->fillRow(row, config);
        }
    }

    LangTableWidget::~LangTableWidget() = default;

    void LangTableWidget::dropEvent(QDropEvent *event) {
        // 获取原行号
        const auto originalRow = this->currentRow();
        // 获取目标行号
        const auto targetRow = this->rowAt(event->position().y());

        // 如果原行号和目标行号相同，不做任何操作
        if (originalRow == targetRow || targetRow == -1) {
            event->ignore();
            return;
        }

        const LangConfig originalConfig = this->exportConfig(originalRow);
        const LangConfig targetConfig = this->exportConfig(targetRow);

        this->fillRow(targetRow, originalConfig);
        this->fillRow(originalRow, targetConfig);
    }

    void LangTableWidget::setCellCheckBox(const int &row, const int &column,
                                          const Qt::CheckState &checkState, const QString &text) {
        auto *widget = new QWidget();
        auto *layout = new QHBoxLayout();
        auto *checkBox = new QCheckBox;

        checkBox->setCheckState(checkState);
        if (!text.isEmpty()) {
            checkBox->setText(text);
        }
        if (column == 0) {
            layout->addWidget(checkBox, 0, Qt::AlignLeft);
        } else {
            layout->addWidget(checkBox, 0, Qt::AlignCenter);
        }
        layout->setContentsMargins(0, 0, 0, 0);
        widget->setLayout(layout);
        this->setCellWidget(row, column, widget);
    }

    bool LangTableWidget::cellCheckState(const int &row, const int &column) const {
        const auto *widget = this->cellWidget(row, column);
        const auto *layout = dynamic_cast<QHBoxLayout *>(widget->layout());
        const auto *checkBox = dynamic_cast<QCheckBox *>(layout->itemAt(0)->widget());
        return checkBox->checkState() == Qt::Checked;
    }

    void LangTableWidget::fillRow(const int &row, const LangConfig &langConfig) {
        this->setCellCheckBox(row, 0, langConfig.enabled ? Qt::Checked : Qt::Unchecked,
                              langConfig.language);
        this->setCellCheckBox(row, 1, langConfig.discardResult ? Qt::Checked : Qt::Unchecked);
        this->setItem(row, 2, new QTableWidgetItem(langConfig.author));
        this->item(row, 2)->setTextAlignment(Qt::AlignCenter);
        this->setItem(row, 3, new QTableWidgetItem(langConfig.description));
        this->item(row, 3)->setTextAlignment(Qt::AlignCenter);
    }

    LangConfig LangTableWidget::exportConfig(const int &row) const {
        LangConfig langConfig;
        langConfig.language = this->item(row, 0)->text();
        langConfig.enabled = this->item(row, 0)->checkState() == Qt::Checked;
        langConfig.discardResult = this->cellCheckState(row, 1);
        langConfig.author = this->item(row, 2)->text();
        langConfig.description = this->item(row, 3)->text();
        return langConfig;
    }
} // LangMgr