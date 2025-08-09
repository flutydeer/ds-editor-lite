#include "G2pListWidget.h"

#include "Model/AppOptions/AppOptions.h"

#include <QDragEnterEvent>
#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <language-manager/ILanguageManager.h>

namespace LangSetting {
    static QPair<QString, QString> extractConfig(const QString &g2pId) {
        const auto firstColonIndex = g2pId.indexOf(':');

        if (firstColonIndex == -1) {
            return {g2pId, "0"};
        }

        QString beforeColon = g2pId.left(firstColonIndex);
        QString afterColon = g2pId.mid(firstColonIndex + 1);

        bool isNumber;
        const int value = afterColon.toInt(&isNumber);

        if (!isNumber || value < 0) {
            return {beforeColon, "0"};
        }

        return {beforeColon, afterColon};
    }

    GListWidget::GListWidget(QWidget *parent) : QListWidget(parent) {
        const auto g2pMgr = LangMgr::ILanguageManager::instance();

        this->setDragDropMode(InternalMove);
        this->setDropIndicatorShown(true);
        this->setSelectionBehavior(SelectRows);

        this->setFocusPolicy(Qt::NoFocus);
        this->setDragDropOverwriteMode(false);

        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        // TODO g2pMgr->g2ps() bug
        const auto langOrder = appOptions->language()->langOrder;
        for (const auto &g2pId : langOrder) {
            const auto &g2p = g2pMgr->g2p(g2pId);
            this->addItem(g2p->displayName());
            this->item(this->count() - 1)->setData(Qt::UserRole, g2p->id());
        }
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    GListWidget::~GListWidget() = default;

    G2pListWidget::G2pListWidget(QWidget *parent) : QWidget(parent) {
        const auto mainLayout = new QVBoxLayout();
        const auto topLayout = new QHBoxLayout();
        const auto g2pLabel = new QLabel(tr("G2p Presets"));

        // auto *copyButton = new QPushButton(tr("Copy"));
        // auto *deleteButton = new QPushButton(tr("Delete"));

        m_gListWidget = new GListWidget();
        m_gListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        topLayout->addWidget(g2pLabel);
        topLayout->addStretch();
        // topLayout->addWidget(copyButton);
        // topLayout->addWidget(deleteButton);
        mainLayout->addLayout(topLayout);
        mainLayout->addWidget(m_gListWidget);
        this->setLayout(mainLayout);

        // connect(copyButton, &QPushButton::clicked, this, &G2pListWidget::copySelectedItem);
        // connect(deleteButton, &QPushButton::clicked, this, &G2pListWidget::deleteSelectedItem);
        // connect(m_gListWidget, &GListWidget::deleteButtonStateChanged, deleteButton,
        //         &QPushButton::setEnabled);
    }

    G2pListWidget::~G2pListWidget() = default;

    QString G2pListWidget::currentG2pId() const {
        return extractConfig(m_gListWidget->currentItem()->data(Qt::UserRole).toString()).first;
    }

    void GListWidget::updateDeleteButtonState() {
        const int currentRow = this->currentRow();
        bool canDelete = false;

        if (currentRow >= 0) {
            const QListWidgetItem *item = this->item(currentRow);
            const auto number = extractConfig(item->data(Qt::UserRole).toString()).second.toInt();
            canDelete = number >= 0;
        }

        emit deleteButtonStateChanged(canDelete);
    }

    void GListWidget::showEvent(QShowEvent *event) {
        QWidget::showEvent(event);
        emit shown();
    }

    void GListWidget::copyItem(int row) {
        // if (row < 0 || row >= this->count())
        //     return;
        //
        // const QListWidgetItem *originalItem = this->item(row);
        // const QString itemName = originalItem->text();
        // const QString oldG2pId =
        //     extractConfig(originalItem->data(Qt::UserRole).toString()).first;
        //
        // int maxNumber = -1;
        // for (int i = 0; i < this->count(); ++i) {
        //     QString dataString = this->item(i)->data(Qt::UserRole).toString();
        //
        //     int number = extractLeadingNumber(dataString).first;
        //     if (number >= 0) {
        //         maxNumber = qMax(maxNumber, number); // 更新最大数字
        //     }
        // }
        //
        // const int newNumber = (maxNumber >= 0) ? maxNumber + 1 : 0;
        // const auto newG2pId = QString::number(newNumber) + oldG2pId;
        //
        // const auto g2pMgr = LangMgr::ILanguageManager::instance();
        // const auto oldG2p = g2pMgr->g2p(oldG2pId);
        // if (oldG2p == nullptr)
        //     return;

        // const auto newG2p =
        //     g2pMgr->addG2p(oldG2p->clone(newG2pId, oldG2p->category(), oldG2p->parent()));
        //
        // if (newG2p) {
        //     auto *newItem = new QListWidgetItem(itemName);
        //     newItem->setData(Qt::UserRole, newG2pId);
        //     this->addItem(newItem);
        // }
    }

    void GListWidget::deleteItem(const int row) {
        if (row < 0 || row >= this->count())
            return;
        delete this->takeItem(row);
    }

    void G2pListWidget::copySelectedItem() const {
        const int currentRow = m_gListWidget->currentRow();
        LangSetting::GListWidget::copyItem(currentRow);
    }

    void G2pListWidget::deleteSelectedItem() const {
        const int currentRow = m_gListWidget->currentRow();
        m_gListWidget->deleteItem(currentRow);
    }

} // LangMgr