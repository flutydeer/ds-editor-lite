#include "LangListWidget.h"

#include <QDragEnterEvent>
#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>

#include "../ILanguageManager.h"

#include <QLabel>

namespace LangMgr {
    LangListWidget::LangListWidget(QWidget *parent) : QListWidget(parent) {
        const auto langMgr = ILanguageManager::instance();

        this->setDragDropMode(InternalMove);
        this->setDropIndicatorShown(true);
        this->setSelectionBehavior(SelectRows);

        this->setFocusPolicy(Qt::NoFocus);
        this->setDragDropOverwriteMode(false);

        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        const auto langConfigs = langMgr->languageConfig();
        for (const auto &config : langConfigs) {
            const auto row = this->count();
            this->setItem(row, config);
        }
    }

    LangListWidget::~LangListWidget() = default;

    LangConfig LangListWidget::currentConfig() const {
        return this->exportConfig(this->currentRow());
    }

    void LangListWidget::dropEvent(QDropEvent *event) {
        const auto originalRow = this->currentRow();
        const auto originalConfig = this->exportConfig(originalRow);

        const auto targetPoint = event->position().toPoint();

        if (!this->visualItemRect(this->itemAt(targetPoint)).contains(targetPoint)) {
            return;
        }

        const auto targetRow = this->indexAt(event->position().toPoint()).row();
        if (targetRow == originalRow) {
            return;
        }

        this->insertItem(targetRow, this->takeItem(originalRow));
        this->setItem(targetRow, originalConfig);
    }


    void LangListWidget::setItem(const int &row, const LangConfig &langConfig) {
        QListWidgetItem *item;
        if (row >= this->count()) {
            item = new QListWidgetItem();
            this->addItem(item);
        } else {
            item = this->item(row);
        }

        auto *widget = new QWidget();

        auto *layout = new QHBoxLayout();
        auto *checkBox = new QCheckBox();
        checkBox->setCheckState(langConfig.enabled ? Qt::Checked : Qt::Unchecked);
        auto *label = new QLabel(langConfig.language);

        auto *discard = new QCheckBox();
        discard->setCheckState(langConfig.discardResult ? Qt::Checked : Qt::Unchecked);
        auto *discardLabel = new QLabel("Discard Result");

        auto *author = new QLabel(langConfig.author);
        author->setVisible(false);
        auto *description = new QLabel(langConfig.description);
        description->setVisible(false);

        layout->addWidget(checkBox);
        layout->addWidget(label);
        layout->addStretch(1);
        layout->addWidget(discard);
        layout->addWidget(discardLabel);
        layout->addWidget(author);
        layout->addWidget(description);
        layout->setContentsMargins(0, 0, 0, 0);

        widget->setLayout(layout);
        this->setItemWidget(item, widget);
    }

    LangConfig LangListWidget::exportConfig(const int &row) const {
        LangConfig langConfig;
        const auto *widget = this->itemWidget(this->item(row));
        const auto *layout = widget->layout();
        langConfig.enabled = layout->itemAt(0)->widget()->property("checked").toBool();
        langConfig.language = layout->itemAt(1)->widget()->property("text").toString();
        langConfig.discardResult = layout->itemAt(3)->widget()->property("checked").toBool();
        langConfig.author = layout->itemAt(5)->widget()->property("text").toString();
        langConfig.description = layout->itemAt(6)->widget()->property("text").toString();
        return langConfig;
    }
} // LangMgr