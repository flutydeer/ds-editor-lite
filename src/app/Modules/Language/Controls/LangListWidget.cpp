#include "LangListWidget.h"

#include <QDragEnterEvent>
#include <QCheckBox>
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
            this->item(this->count() - 1)->setData(Qt::UserRole, config.id);
        }
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    LangListWidget::~LangListWidget() = default;

    QStringList LangListWidget::langOrder() const {
        QStringList order;
        for (int i = 0; i < this->count(); ++i) {
            order << this->item(i)->data(Qt::UserRole).toString();
        }
        return order;
    }

    void LangListWidget::dropEvent(QDropEvent *event) {
        QListWidget::dropEvent(event);
        const auto langMgr = ILanguageManager::instance();
        langMgr->setLanguageOrder(this->langOrder());
    }
} // LangMgr