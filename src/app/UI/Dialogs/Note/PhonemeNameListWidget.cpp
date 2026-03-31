//
// Created by FlutyDeer on 2026/4/1.
//

#include "PhonemeNameListWidget.h"

#include "PhonemeNameItemView.h"
#include "Model/NoteDialog/PhonemeNameListModel.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Views/Common/LanguageComboBox.h"

PhonemeNameListWidget::PhonemeNameListWidget(QWidget *parent) : QListWidget(parent) {
    setSelectionMode(QAbstractItemView::NoSelection);
}

void PhonemeNameListWidget::setModel(PhonemeNameListModel *model) {
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    m_model = model;
    if (m_model) {
        connect(m_model, &QAbstractListModel::modelReset, this, &PhonemeNameListWidget::refreshItems);
        connect(m_model, &QAbstractListModel::rowsInserted, this, [this](const QModelIndex &, int first, int last) {
            for (int i = first; i <= last; i++) {
                auto item = new QListWidgetItem(this);
                item->setSizeHint(QSize(0, 32));
                insertItem(i, item);
                updateItemWidget(i);
            }
        });
        connect(m_model, &QAbstractListModel::rowsRemoved, this, [this](const QModelIndex &, int first, int last) {
            for (int i = last; i >= first; i--) {
                delete takeItem(i);
            }
        });
        connect(m_model, &QAbstractListModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
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

        connect(widget->cbLanguage(), &QComboBox::currentTextChanged, this, [this, row](const QString &text) {
            if (m_model) {
                m_model->setData(m_model->index(row), text, PhonemeNameListModel::LanguageRole);
            }
        });
        connect(widget->leName(), &QLineEdit::textChanged, this, [this, row](const QString &text) {
            if (m_model) {
                m_model->setData(m_model->index(row), text, PhonemeNameListModel::NameRole);
            }
        });
        connect(widget->cbIsOnset(), &QCheckBox::toggled, this, [this, row](bool checked) {
            if (m_model) {
                m_model->setData(m_model->index(row), checked, PhonemeNameListModel::IsOnsetRole);
            }
        });
    }

    QModelIndex index = m_model->index(row);
    widget->blockSignals(true);
    widget->cbLanguage()->setCurrentText(m_model->data(index, PhonemeNameListModel::LanguageRole).toString());
    widget->leName()->setText(m_model->data(index, PhonemeNameListModel::NameRole).toString());
    widget->cbIsOnset()->setChecked(m_model->data(index, PhonemeNameListModel::IsOnsetRole).toBool());
    widget->blockSignals(false);
}
