#include "LangListWidget.h"

#include <QDragEnterEvent>
#include <QCheckBox>
#include <QHeaderView>

#include <language-manager/ILanguageManager.h>

namespace LangSetting {
    LangListWidget::LangListWidget(QWidget *parent) : QListWidget(parent) {
        const auto langMgr = LangMgr::ILanguageManager::instance();

        this->setDragDropMode(InternalMove);
        this->setDropIndicatorShown(true);
        this->setSelectionBehavior(SelectRows);

        this->setFocusPolicy(Qt::NoFocus);
        this->setDragDropOverwriteMode(false);

        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        const auto langOrder = langMgr->defaultOrder();
        for (const auto &lang : langOrder) {
            const auto langFactory = langMgr->language(lang);
            this->addItem(langFactory->displayName());
            this->item(this->count() - 1)->setData(Qt::UserRole, langFactory->id());
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

    void LangListWidget::showEvent(QShowEvent *event) {
        QWidget::showEvent(event);
        emit shown();
    }

    void LangListWidget::dropEvent(QDropEvent *event) {
        QListWidget::dropEvent(event);
        const auto langMgr = LangMgr::ILanguageManager::instance();
        langMgr->setDefaultOrder(this->langOrder());
        Q_EMIT this->priorityChanged();
    }
} // LangMgr