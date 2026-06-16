#include "SpeakerMixList.h"
#include "SpeakerMixBar.h"
#include "UI/Controls/ColorDot.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <utility>

namespace {
    void setWidgetEnabledStyle(QWidget *widget, const bool enabled) {
        widget->setEnabled(enabled);
        widget->setProperty("mixEnabled", enabled);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
}

SpeakerMixList::SpeakerMixList(const QString &packageName, const QStringList &speakerTypes,
                               QWidget *parent)
    : QListWidget(parent), m_packageName(packageName), m_speakerTypes(speakerTypes),
      m_mixBar(new SpeakerMixBar(this)), m_sourceEditingEnabled(true) {
    m_speakerTypes = speakerTypes.empty() ? QStringList({"no singer"}) : speakerTypes;

    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::NoFrame);
    setSpacing(8);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(model(), &QAbstractItemModel::rowsMoved, this, &SpeakerMixList::onItemOrderChanged);
    connect(m_mixBar, &SpeakerMixBar::valuesChanged, this, &SpeakerMixList::syncRowsFromBar);

    for (const QString &speakerType : std::as_const(m_speakerTypes)) {
        createRow(speakerType);
    }

    if (m_rows.isEmpty()) {
        createRow();
    }

    const int baseValue = 100 / m_rows.size();
    const int remainder = 100 % m_rows.size();
    QVector<int> values;
    for (int i = 0; i < m_rows.size(); ++i) {
        values.append(baseValue + (i < remainder ? 1 : 0));
    }
    setRowsValues(values);
    updateBarLabelsAndColors();
}

void SpeakerMixList::setSpeakerTypes(const QStringList &speakerTypes) {
    m_speakerTypes = speakerTypes.empty() ? QStringList({"no singer"}) : speakerTypes;

    for (auto &row : m_rows) {
        const QSignalBlocker blocker(row.speakerComboBox);
        row.speakerComboBox->clear();
        row.speakerComboBox->addItems(m_speakerTypes);
        if (!m_speakerTypes.contains(row.speakerComboBox->currentText())) {
            row.speakerComboBox->setCurrentText(m_speakerTypes.first());
        }
        updateRowColor(row);
    }

    updateBarLabelsAndColors();
}

void SpeakerMixList::setSourceEditingEnabled(const bool enabled) {
    if (m_sourceEditingEnabled == enabled)
        return;

    m_sourceEditingEnabled = enabled;
    setDragDropMode(enabled ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);

    for (auto &row : m_rows) {
        row.dragHandle->setEnabled(enabled);
        row.dragHandle->setCursor(enabled ? Qt::SizeAllCursor : Qt::ArrowCursor);
        setWidgetEnabledStyle(row.speakerComboBox, enabled);
        setWidgetEnabledStyle(row.deleteButton, enabled && m_rows.size() > 1);
    }
}

QWidget *SpeakerMixList::createRowWidget(const QString &speakerType) {
    const auto widget = new QWidget(this);
    widget->setFixedHeight(28);

    const auto layout = new QHBoxLayout(widget);
    layout->setSpacing(8);
    layout->setContentsMargins(0, 0, 0, 0);

    const auto dragHandle = new QLabel("=", widget);
    dragHandle->setFixedSize(28, 28);
    dragHandle->setCursor(Qt::SizeAllCursor);
    dragHandle->setAlignment(Qt::AlignCenter);

    const auto colorDot = new ColorDot(defaultColors()[m_rows.size() % defaultColors().size()],
                                       widget);
    colorDot->setFixedSize(10, 10);

    const auto typeLabel = new QLabel("声线", widget);
    typeLabel->setFixedHeight(28);

    const auto speakerComboBox = new QComboBox(widget);
    speakerComboBox->addItems(m_speakerTypes);
    speakerComboBox->setCurrentText(speakerType);
    speakerComboBox->setFixedWidth(80);
    connect(speakerComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SpeakerMixList::onSpeakerTypeChanged);

    const auto positionLabel = new QLabel("位置", widget);
    positionLabel->setFixedHeight(28);
    positionLabel->setAlignment(Qt::AlignCenter);
    positionLabel->setFixedWidth(84);
    positionLabel->setStyleSheet(
        "color: #777B84; background: #252932; border: 1px solid #363B46;"
        "border-radius: 3px; padding: 0 8px;");

    const auto deleteButton = new QPushButton("-", widget);
    deleteButton->setFixedSize(28, 28);
    connect(deleteButton, &QPushButton::clicked, this, &SpeakerMixList::removeRow);

    layout->addWidget(dragHandle);
    layout->addWidget(colorDot);
    layout->addWidget(typeLabel);
    layout->addWidget(speakerComboBox);
    layout->addStretch();
    layout->addWidget(positionLabel);
    layout->addStretch();
    layout->addWidget(deleteButton);

    RowComponents row;
    row.container = widget;
    row.layout = layout;
    row.dragHandle = dragHandle;
    row.colorDot = colorDot;
    row.speakerComboBox = speakerComboBox;
    row.positionLabel = positionLabel;
    row.deleteButton = deleteButton;
    row.color = defaultColors()[m_rows.size() % defaultColors().size()];
    updateRowColor(row);
    m_rows.append(row);

    return widget;
}

void SpeakerMixList::createRow(const QString &speakerType) {
    const auto item = new QListWidgetItem(this);
    QWidget *widget = createRowWidget(speakerType);
    item->setSizeHint(QSize(0, 28));
    setItemWidget(item, widget);
}

void SpeakerMixList::addRow() {
    if (!m_sourceEditingEnabled)
        return;

    const int nextIndex = m_rows.size() % m_speakerTypes.size();
    createRow(m_speakerTypes.value(nextIndex, m_speakerTypes.first()));
    syncRowsWithListItems();

    QVector<int> values = m_mixBar->getValues();
    values.append(0);
    setRowsValues(values);
    setSourceEditingEnabled(m_sourceEditingEnabled);
}

void SpeakerMixList::removeRow() {
    if (!m_sourceEditingEnabled || m_rows.size() <= 1)
        return;

    const auto deleteButton = qobject_cast<QPushButton *>(sender());
    const int rowIndexToRemove = findRowIndexBySender(deleteButton);
    if (rowIndexToRemove == -1)
        return;

    const QVector<int> previousValues = m_mixBar->getValues();
    const int removedValue = previousValues.value(rowIndexToRemove);

    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *listItem = item(i);
        if (listItem && itemWidget(listItem) == m_rows[rowIndexToRemove].container) {
            delete takeItem(i);
            break;
        }
    }
    m_rows.removeAt(rowIndexToRemove);

    QVector<int> values;
    const int remainingTotal = 100 - removedValue;
    if (remainingTotal > 0) {
        int allocated = 0;
        for (int i = 0; i < previousValues.size(); ++i) {
            if (i == rowIndexToRemove)
                continue;
            const int value = previousValues[i] + previousValues[i] * removedValue / remainingTotal;
            values.append(value);
            allocated += value;
        }
        if (!values.isEmpty()) {
            values[0] += 100 - allocated;
        }
    } else if (!m_rows.isEmpty()) {
        values = QVector<int>(m_rows.size(), 0);
        values[0] = 100;
    }

    setRowsValues(values);
    setSourceEditingEnabled(m_sourceEditingEnabled);
}

void SpeakerMixList::onItemOrderChanged() {
    syncRowsWithListItems();
    syncRowsFromBar(m_mixBar->getValues());
}

void SpeakerMixList::onSpeakerTypeChanged() {
    const int rowIndex = findRowIndexBySender(sender());
    if (rowIndex != -1) {
        updateRowColor(m_rows[rowIndex]);
    }
    updateBarLabelsAndColors();
}

void SpeakerMixList::syncRowsFromBar(const QVector<int> &values) {
    int cumulative = 0;
    for (int i = 0; i < m_rows.size() && i < values.size(); ++i) {
        m_rows[i].positionLabel->setText(QString::number(cumulative) + "%");
        cumulative += values[i];
    }
    updateBarLabelsAndColors();
}

void SpeakerMixList::setRowsValues(const QVector<int> &values) {
    m_mixBar->setValues(values);

    int cumulative = 0;
    for (int i = 0; i < m_rows.size() && i < values.size(); ++i) {
        m_rows[i].positionLabel->setText(QString::number(cumulative) + "%");
        cumulative += values[i];
    }
}

void SpeakerMixList::syncRowsWithListItems() {
    QVector<RowComponents> syncedRows;

    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *listItem = item(i);
        if (!listItem)
            continue;

        const QWidget *widget = itemWidget(listItem);
        for (const auto &row : m_rows) {
            if (row.container == widget) {
                syncedRows.append(row);
                break;
            }
        }
    }

    m_rows = syncedRows;
}

int SpeakerMixList::findRowIndexBySender(const QObject *object) const {
    if (!object)
        return -1;

    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].speakerComboBox == object || m_rows[i].deleteButton == object) {
            return i;
        }
    }
    return -1;
}

void SpeakerMixList::updateRowColor(RowComponents &row) {
    const int colorIndex = m_speakerTypes.indexOf(row.speakerComboBox->currentText());
    row.color = defaultColors().value(colorIndex, defaultColors().first());
    static_cast<ColorDot *>(row.colorDot)->setColor(row.color);
}

void SpeakerMixList::updateBarLabelsAndColors() {
    m_mixBar->setLabels(getLabels());
    m_mixBar->setSegmentColors(getColors());
}

QVector<QColor> SpeakerMixList::getColors() const {
    QVector<QColor> colors;
    colors.reserve(m_rows.size());
    for (const auto &row : m_rows) {
        colors.append(row.color);
    }
    return colors;
}

QVector<QColor> SpeakerMixList::defaultColors() {
    return {QColor("#9BBAFF"), QColor("#DEA4E0"), QColor("#F7A199"), QColor("#DAB669"),
            QColor("#90CD92"), QColor("#8CC8FF"), QColor("#E9A86A"), QColor("#83C8B3")};
}

QVector<int> SpeakerMixList::getValues() const {
    if (m_mixBar) {
        return m_mixBar->getValues();
    }
    return {};
}

QVector<QString> SpeakerMixList::getLabels() const {
    QVector<QString> labels;
    labels.reserve(m_rows.size());
    for (const auto &row : m_rows) {
        labels.append(row.speakerComboBox->currentText());
    }
    return labels;
}
