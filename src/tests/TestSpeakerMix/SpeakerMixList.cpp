#include "SpeakerMixList.h"
#include "SpeakerMixBar.h"
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

SpeakerMixList::SpeakerMixList(const QString &packageName, const QStringList &speakerTypes,
                               QWidget *parent)
    : QListWidget(parent), m_packageName(packageName), m_speakerTypes(speakerTypes) {
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSpacing(2);

    m_speakerTypes = speakerTypes.empty() ? QStringList({"no singer"}) : speakerTypes;

    m_mixBar = new SpeakerMixBar(this);
    connect(model(), &QAbstractItemModel::rowsMoved, this, &SpeakerMixList::onItemOrderChanged);

    if (!m_speakerTypes.isEmpty()) {
        createRow(m_speakerTypes.first());
        updateBarValues();
    }
}

void SpeakerMixList::setSpeakerTypes(const QStringList &speakerTypes) {
    m_speakerTypes = speakerTypes;

    for (const auto &row : m_rows) {
        row.speakerComboBox->clear();
        row.speakerComboBox->addItems(m_speakerTypes);
    }
}

QWidget *SpeakerMixList::createRowWidget(const QString &speakerType) {
    const auto widget = new QWidget(this);
    const auto layout = new QHBoxLayout(widget);
    layout->setSpacing(5);
    layout->setContentsMargins(5, 5, 5, 5);

    const auto dragHandle = new QLabel("≡", widget);
    dragHandle->setFixedSize(25, 25);
    dragHandle->setCursor(Qt::SizeAllCursor);
    dragHandle->setAlignment(Qt::AlignCenter);

    const auto speakerComboBox = new QComboBox(widget);
    speakerComboBox->addItems(m_speakerTypes);
    speakerComboBox->setCurrentText(speakerType);
    speakerComboBox->setMaximumWidth(150);
    connect(speakerComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SpeakerMixList::onSpeakerTypeChanged);

    const auto deleteButton = new QPushButton("×", widget);
    deleteButton->setFixedSize(25, 25);
    deleteButton->setStyleSheet("QPushButton { border: 1px solid gray; background: #ff6b6b; color: "
                                "white; font-weight: bold; }");
    connect(deleteButton, &QPushButton::clicked, this, &SpeakerMixList::removeRow);

    layout->addWidget(dragHandle);
    const auto comboLabel = new QLabel(m_packageName + ": ", widget);
    layout->addWidget(comboLabel);
    layout->addWidget(speakerComboBox);
    layout->addStretch();
    layout->addWidget(deleteButton);

    RowComponents row;
    row.container = widget;
    row.layout = layout;
    row.speakerComboBox = speakerComboBox;
    row.deleteButton = deleteButton;
    m_rows.append(row);

    return widget;
}

void SpeakerMixList::createRow(const QString &speakerType) {
    const auto item = new QListWidgetItem(this);
    QWidget *widget = createRowWidget(speakerType);
    item->setSizeHint(widget->sizeHint());
    setItemWidget(item, widget);
}

void SpeakerMixList::addRow() {
    const QVector<int> currentValues = m_mixBar->getValues();
    createRow(m_speakerTypes.first());
    syncRowsWithListItems();
    updateBarValues(currentValues, true);
}

void SpeakerMixList::removeRow() {
    if (m_rows.size() <= 1)
        return;

    const auto deleteButton = qobject_cast<QPushButton *>(sender());
    if (!deleteButton)
        return;

    int rowIndexToRemove = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].deleteButton == deleteButton) {
            rowIndexToRemove = i;
            break;
        }
    }

    if (rowIndexToRemove == -1)
        return;

    const QVector<int> currentValues = m_mixBar->getValues();

    const QListWidgetItem *itemToRemove = nullptr;
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *item = this->item(i);
        if (item && itemWidget(item) == m_rows[rowIndexToRemove].container) {
            itemToRemove = item;
            break;
        }
    }

    if (itemToRemove) {
        delete takeItem(row(itemToRemove));
        m_rows.removeAt(rowIndexToRemove);
        updateBarValues(currentValues, false, rowIndexToRemove);
    }
}

void SpeakerMixList::onItemOrderChanged() {
    syncRowsWithListItems();
    updateBarValues();
}

void SpeakerMixList::onSpeakerTypeChanged() {
    updateBarLabels();
}

void SpeakerMixList::syncRowsWithListItems() {
    QVector<RowComponents> syncedRows;

    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *item = this->item(i);
        if (!item)
            continue;

        const QWidget *widget = itemWidget(item);
        if (!widget)
            continue;

        for (const auto &row : m_rows) {
            if (row.container == widget) {
                syncedRows.append(row);
                break;
            }
        }
    }

    m_rows = syncedRows;
}

int SpeakerMixList::findRowIndexByWidget(const QWidget *widget) const {
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].container == widget) {
            return i;
        }
    }
    return -1;
}

void SpeakerMixList::updateBarValues(const QVector<int> &previousValues, const bool isAddOperation,
                                     const int removedIndex) {
    if (!m_mixBar)
        return;

    const int count = this->count();
    QVector<int> values;
    QVector<QString> labels;

    if (count > 0) {
        if (isAddOperation && !previousValues.isEmpty()) {
            const int n = count;
            const int newSpeakerValue = 100 / n;
            const int remainingTotal = 100 - newSpeakerValue;

            int previousSum = 0;
            for (const int value : previousValues) {
                previousSum += value;
            }

            values.resize(count - 1);
            int allocated = 0;
            for (int i = 0; i < count - 1; ++i) {
                if (previousSum > 0) {
                    values[i] = previousValues[i] * remainingTotal / previousSum;
                } else {
                    values[i] = remainingTotal / (count - 1);
                }
                allocated += values[i];
            }

            if (allocated != remainingTotal) {
                values[0] += remainingTotal - allocated;
            }

            values.append(newSpeakerValue);

        } else if (!isAddOperation && removedIndex != -1 && !previousValues.isEmpty()) {
            const int removedValue = previousValues[removedIndex];

            values.resize(count);
            int allocated = 0;

            for (int i = 0, j = 0; i < previousValues.size(); ++i) {
                if (i != removedIndex) {
                    const int additional = previousValues[i] * removedValue / (100 - removedValue);
                    values[j] = previousValues[i] + additional;
                    allocated += values[j];
                    j++;
                }
            }

            if (allocated != 100) {
                values[0] += 100 - allocated;
            }

        } else {
            const int baseValue = 100 / count;
            const int remainder = 100 % count;

            for (int i = 0; i < count; ++i) {
                const int value = baseValue + (i < remainder ? 1 : 0);
                values.append(value);
            }
        }

        for (int i = 0; i < count; ++i) {
            const QWidget *widget = itemWidget(item(i));
            const int rowIndex = findRowIndexByWidget(widget);
            if (rowIndex != -1) {
                labels.append(m_rows[rowIndex].speakerComboBox->currentText());
            }
        }
    }

    m_mixBar->setValues(values);
    m_mixBar->setLabels(labels);
}

void SpeakerMixList::updateBarLabels() {
    if (!m_mixBar)
        return;

    QVector<QString> labels;
    for (const auto &row : m_rows) {
        labels.append(row.speakerComboBox->currentText());
    }
    m_mixBar->setLabels(labels);
}

QVector<int> SpeakerMixList::getValues() const {
    if (m_mixBar) {
        return m_mixBar->getValues();
    }
    return QVector<int>();
}

QVector<QString> SpeakerMixList::getLabels() const {
    QVector<QString> labels;
    for (const auto &row : m_rows) {
        labels.append(row.speakerComboBox->currentText());
    }
    return labels;
}