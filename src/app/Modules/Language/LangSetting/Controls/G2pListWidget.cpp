#include "G2pListWidget.h"
#include "LangListWidget.h"

#include <QDragEnterEvent>
#include <QCheckBox>
#include <QHeaderView>
#include <language-manager/IG2pManager.h>

namespace LangSetting {
    G2pListWidget::G2pListWidget(QWidget *parent) : QListWidget(parent) {
        const auto g2pMgr = LangMgr::IG2pManager::instance();

        this->setDragDropMode(InternalMove);
        this->setDropIndicatorShown(true);
        this->setSelectionBehavior(SelectRows);

        this->setFocusPolicy(Qt::NoFocus);
        this->setDragDropOverwriteMode(false);

        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        for (const auto &lang : g2pMgr->g2ps()) {
            this->addItem(lang->displayName());
            this->item(this->count() - 1)->setData(Qt::UserRole, lang->id());
        }
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    G2pListWidget::~G2pListWidget() = default;

    void G2pListWidget::showEvent(QShowEvent *event) {
        QWidget::showEvent(event);
        emit shown();
    }

} // LangMgr