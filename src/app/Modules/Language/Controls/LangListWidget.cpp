#include "LangListWidget.h"

#include <QDragEnterEvent>
#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>

#include "../LangMgr/ILanguageManager.h"

namespace LangMgr {
    LangListWidget::LangListWidget(QWidget *parent) : QListWidget(parent) {
        const auto langMgr = ILanguageManager::instance();

        this->setDragDropMode(InternalMove);
        this->setDropIndicatorShown(true);
        this->setSelectionBehavior(SelectRows);

        this->setFocusPolicy(Qt::NoFocus);
        this->setDragDropOverwriteMode(false);

        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        const auto langConfigs = langMgr->languageConfigs();
        for (const auto &config : langConfigs) {
            this->addItem(config.language);
        }
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    LangListWidget::~LangListWidget() = default;
} // LangMgr