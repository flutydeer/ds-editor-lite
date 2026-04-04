//
// Created by FlutyDeer on 2026/4/1.
//

#include "PhonemeNameListWidget.h"

#include "PhonemeNameItemView.h"
#include "Model/NoteDialog/PhonemeNameListModel.h"
#include "Model/NoteDialog/PhonemeNameItemModel.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include <QMWidgets/cmenu.h>

PhonemeNameListWidget::PhonemeNameListWidget(QWidget *parent) : QListWidget(parent) {
    setSelectionMode(QAbstractItemView::NoSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setMinimumWidth(400);
    setVerticalScrollMode(ScrollPerPixel);
    connect(this, &QListWidget::customContextMenuRequested, this,
            &PhonemeNameListWidget::onCustomContextMenuRequested);

    m_contextMenu = new CMenu(this);

    auto insertAboveAction = m_contextMenu->addAction("Insert Above");
    auto insertBelowAction = m_contextMenu->addAction("Insert Below");
    auto deleteAction = m_contextMenu->addAction("Delete");

    connect(insertAboveAction, &QAction::triggered, this, &PhonemeNameListWidget::insertAbove);
    connect(insertBelowAction, &QAction::triggered, this, &PhonemeNameListWidget::insertBelow);
    connect(deleteAction, &QAction::triggered, this, &PhonemeNameListWidget::deleteItem);
}

void PhonemeNameListWidget::setModel(PhonemeNameListModel *model) {
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    m_model = model;
    if (m_model) {
        connect(m_model, &QAbstractListModel::modelReset, this,
                &PhonemeNameListWidget::refreshItems);
        connect(m_model, &QAbstractListModel::rowsInserted, this,
                [this](const QModelIndex &, int first, int last) {
                    for (int i = first; i <= last; i++) {
                        auto item = new QListWidgetItem();
                        item->setSizeHint(QSize(0, 32));
                        insertItem(i, item);
                        updateItemWidget(i);
                    }
                });
        connect(m_model, &QAbstractListModel::rowsRemoved, this,
                [this](const QModelIndex &, int first, int last) {
                    for (int i = last; i >= first; i--) {
                        delete takeItem(i);
                    }
                });
        connect(m_model, &QAbstractListModel::dataChanged, this,
                [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
                    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
                        updateItemWidget(i);
                    }
                });
        refreshItems();
    }
}

PhonemeNameListModel *PhonemeNameListWidget::model() const {
    return m_model;
}

void PhonemeNameListWidget::refreshItems() {
    clear();
    if (!m_model)
        return;
    for (int i = 0; i < m_model->rowCount({}); i++) {
        auto item = new QListWidgetItem(this);
        item->setSizeHint(QSize(0, 32));
        addItem(item);
        updateItemWidget(i);
    }
}

void PhonemeNameListWidget::updateItemWidget(int row) {
    if (!m_model || row < 0 || row >= m_model->rowCount({}))
        return;

    auto widget = qobject_cast<PhonemeNameItemView *>(itemWidget(item(row)));
    if (!widget) {
        widget = new PhonemeNameItemView;
        setItemWidget(item(row), widget);

        auto listItem = item(row);
        connect(widget->cbLanguage(), &QComboBox::currentTextChanged, this,
                [this, listItem](const QString &text) {
                    if (m_model) {
                        int currentRow = this->row(listItem);
                        if (currentRow >= 0)
                            m_model->setData(m_model->index(currentRow), text,
                                             PhonemeNameListModel::LanguageRole);
                    }
                });
        connect(widget->leName(), &QLineEdit::textChanged, this,
                [this, listItem](const QString &text) {
                    if (m_model) {
                        int currentRow = this->row(listItem);
                        if (currentRow >= 0)
                            m_model->setData(m_model->index(currentRow), text,
                                             PhonemeNameListModel::NameRole);
                    }
                });
        connect(widget->cbIsOnset(), &QCheckBox::toggled, this, [this, listItem](bool checked) {
            if (m_model) {
                int currentRow = this->row(listItem);
                if (currentRow >= 0)
                    m_model->setData(m_model->index(currentRow), checked,
                                     PhonemeNameListModel::IsOnsetRole);
            }
        });
        connect(widget, &PhonemeNameItemView::insertAboveClicked, this, [this, listItem]() {
            int currentRow = this->row(listItem);
            if (currentRow >= 0)
                insertItemAt(currentRow, currentRow);
        });
        connect(widget, &PhonemeNameItemView::deleteClicked, this, [this, listItem]() {
            if (m_model) {
                int currentRow = this->row(listItem);
                if (currentRow >= 0)
                    m_model->removeItem(currentRow);
            }
        });
    }

    QModelIndex index = m_model->index(row);
    widget->blockSignals(true);
    widget->cbLanguage()->setCurrentText(
        m_model->data(index, PhonemeNameListModel::LanguageRole).toString());
    widget->leName()->setText(m_model->data(index, PhonemeNameListModel::NameRole).toString());
    widget->cbIsOnset()->setChecked(
        m_model->data(index, PhonemeNameListModel::IsOnsetRole).toBool());
    widget->blockSignals(false);
}

void PhonemeNameListWidget::onCustomContextMenuRequested(const QPoint &pos) {
    if (!m_model) {
        return;
    }

    QListWidgetItem *item = itemAt(pos);
    if (item) {
        m_contextMenuRow = row(item);
        m_contextMenu->exec(mapToGlobal(pos));
    }
}

void PhonemeNameListWidget::insertAbove() {
    if (m_contextMenuRow >= 0)
        insertItemAt(m_contextMenuRow, m_contextMenuRow);
}

void PhonemeNameListWidget::insertBelow() {
    if (m_contextMenuRow >= 0)
        insertItemAt(m_contextMenuRow + 1, m_contextMenuRow);
}

void PhonemeNameListWidget::deleteItem() {
    if (m_contextMenuRow >= 0 && m_model)
        m_model->removeItem(m_contextMenuRow);
}

PhonemeNameItemModel PhonemeNameListWidget::createNewItem(int sourceRow) const {
    PhonemeNameItemModel item;
    if (m_model && sourceRow >= 0 && sourceRow < m_model->rowCount({})) {
        QString language =
            m_model->data(m_model->index(sourceRow), PhonemeNameListModel::LanguageRole).toString();
        item.setLanguage(language);
    }
    item.setName("");
    item.setIsOnset(false);
    return item;
}

void PhonemeNameListWidget::insertItemAt(int targetRow, int sourceRow) {
    if (m_model)
        m_model->insertItem(targetRow, createNewItem(sourceRow));
}
