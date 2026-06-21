#include "SpeakerMixList.h"
#include "SpeakerMixBar.h"
#include "UI/Controls/ColorDot.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Utils/SpeakerMixColorResolver.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSet>
#include <QSignalBlocker>
#include <QFontMetrics>
#include <QStandardItemModel>
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
                               const QList<SpeakerInfo> &referenceSpeakers, QWidget *parent)
    : QListWidget(parent), m_packageName(packageName), m_speakerTypes(speakerTypes),
      m_referenceSpeakers(referenceSpeakers), m_mixBar(new SpeakerMixBar(this)),
      m_sourceEditingEnabled(true) {
    m_speakerTypes = speakerTypes.empty() ? QStringList({"no singer"}) : speakerTypes;

    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::NoFrame);
    setSpacing(0);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    connect(model(), &QAbstractItemModel::rowsMoved, this, &SpeakerMixList::onItemOrderChanged);
    connect(m_mixBar, &SpeakerMixBar::valuesChanged, this, &SpeakerMixList::syncRowsFromBar);
}

void SpeakerMixList::setSpeakerTypes(const QStringList &speakerTypes) {
    m_speakerTypes = speakerTypes.empty() ? QStringList({"no singer"}) : speakerTypes;

    for (auto &row : m_rows) {
        if (!m_speakerTypes.contains(row.speakerName))
            row.speakerName = m_speakerTypes.first();
        updateRowColor(row);
    }

    refreshComboBoxItems();
    updateBarLabelsAndColors();
}

void SpeakerMixList::setSpeakerDisplayNames(const QMap<QString, QString> &displayNames) {
    m_speakerDisplayNames = displayNames;
    refreshComboBoxItems();
    updateBarLabelsAndColors();
}

void SpeakerMixList::setSourceEditingEnabled(const bool enabled) {
    if (m_sourceEditingEnabled == enabled)
        return;

    m_sourceEditingEnabled = enabled;
    setDragEnabled(enabled);
    setDragDropMode(enabled ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);

    for (auto &row : m_rows) {
        row.dragHandle->setEnabled(enabled);
        row.dragHandle->setCursor(enabled ? Qt::SizeAllCursor : Qt::ArrowCursor);
        setWidgetEnabledStyle(row.speakerComboBox, enabled);
    }
}

void SpeakerMixList::setDoubleValues(const QVector<double> &values) {
    setRowsValues(values);
}

void SpeakerMixList::addSpeaker(const QString &speakerName) {
    if (findRowIndexBySpeaker(speakerName) != -1)
        return;

    const QVector<double> previousValues = m_mixBar->getDoubleValues();
    const int previousCount = m_rows.size();

    createRow(speakerName);

    if (previousCount == 0) {
        setRowsValues({100.0});
    } else {
        const double newValue = 100.0 / m_rows.size();
        const double remainingTotal = 100.0 - newValue;

        double previousTotal = 0;
        for (const double value : previousValues)
            previousTotal += value;

        QVector<double> values;
        values.reserve(m_rows.size());
        if (qFuzzyIsNull(previousTotal)) {
            const double baseValue = remainingTotal / previousCount;
            for (int i = 0; i < previousCount; ++i)
                values.append(baseValue);
        } else {
            for (const double value : previousValues)
                values.append(value * remainingTotal / previousTotal);
        }
        values.append(newValue);
        setRowsValues(values);
    }

    refreshComboBoxItems();
    updateBarLabelsAndColors();
}

void SpeakerMixList::removeSpeaker(const QString &speakerName) {
    if (m_rows.size() <= 1)
        return;

    const int rowIndex = findRowIndexBySpeaker(speakerName);
    if (rowIndex == -1)
        return;

    const QVector<double> previousValues = m_mixBar->getDoubleValues();
    const double removedValue = previousValues.value(rowIndex);

    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *listItem = item(i);
        if (listItem && itemWidget(listItem) == m_rows[rowIndex].container) {
            delete takeItem(i);
            break;
        }
    }
    m_rows.removeAt(rowIndex);

    QVector<double> values;
    const double remainingTotal = 100.0 - removedValue;
    if (remainingTotal > 0) {
        for (int i = 0; i < previousValues.size(); ++i) {
            if (i == rowIndex)
                continue;
            values.append(previousValues[i] + previousValues[i] * removedValue / remainingTotal);
        }
    } else if (!m_rows.isEmpty()) {
        values = QVector<double>(m_rows.size(), 0);
        values[0] = 100;
    }

    setRowsValues(values);
    refreshComboBoxItems();
    updateBarLabelsAndColors();
}

void SpeakerMixList::setSpeakers(const QVector<QString> &speakerNames) {
    clear();
    m_rows.clear();

    for (const auto &speakerName : speakerNames)
        createRow(speakerName);

    if (m_rows.isEmpty())
        createRow(m_speakerTypes.value(0));

    redistributeValues();
    refreshComboBoxItems();
    updateBarLabelsAndColors();
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
    dragHandle->installEventFilter(this);

    const auto colorDot = new ColorDot(
        SpeakerMixColorResolver::colorsForSpeaker(speakerType, m_referenceSpeakers, m_rows.size())
            .accent,
        widget);
    colorDot->setFixedSize(10, 10);

    const auto typeLabel = new QLabel("声线: ", widget);
    typeLabel->setFixedHeight(28);

    const auto speakerComboBox = new ComboBox(widget);
    speakerComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    for (const auto &speakerName : std::as_const(m_speakerTypes))
        speakerComboBox->addItem(speakerDisplayName(speakerName), speakerName);
    const int currentIndex = speakerComboBox->findData(speakerType, Qt::UserRole);
    if (currentIndex >= 0)
        speakerComboBox->setCurrentIndex(currentIndex);
    speakerComboBox->setFixedWidth(120);
    connect(speakerComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SpeakerMixList::onSpeakerTypeChanged);

    const auto positionPrefixLabel = new QLabel("位置: ", widget);
    positionPrefixLabel->setFixedHeight(28);

    const auto positionLabel = new QLabel("0%", widget);
    positionLabel->setFixedHeight(28);
    positionLabel->setAlignment(Qt::AlignCenter);
    positionLabel->setFixedWidth(84);
    positionLabel->setStyleSheet("color: #777B84; background: #252932; border: 1px solid #363B46;"
                                 "border-radius: 3px; padding: 0 8px;");

    layout->addWidget(dragHandle);
    layout->addWidget(colorDot);
    layout->addWidget(typeLabel);
    layout->addWidget(speakerComboBox);
    layout->addWidget(positionPrefixLabel);
    layout->addWidget(positionLabel);
    layout->addStretch();

    RowComponents row;
    row.container = widget;
    row.layout = layout;
    row.dragHandle = dragHandle;
    row.colorDot = colorDot;
    row.speakerComboBox = speakerComboBox;
    row.positionLabel = positionLabel;
    row.color =
        SpeakerMixColorResolver::colorsForSpeaker(speakerType, m_referenceSpeakers, m_rows.size())
            .accent;
    row.speakerName = speakerType;
    updateRowColor(row);
    m_rows.append(row);

    return widget;
}

void SpeakerMixList::createRow(const QString &speakerType) {
    const auto item = new QListWidgetItem(this);
    QWidget *widget = createRowWidget(speakerType);
    item->setSizeHint(QSize(0, 36));
    setItemWidget(item, widget);
}

void SpeakerMixList::redistributeValues() {
    const int n = m_rows.size();
    if (n == 0)
        return;
    const int baseValue = 100 / n;
    const int remainder = 100 % n;
    QVector<double> values;
    for (int i = 0; i < n; ++i) {
        values.append(baseValue + (i < remainder ? 1 : 0));
    }
    setRowsValues(values);
}

void SpeakerMixList::onItemOrderChanged() {
    const QVector<double> previousValues = m_mixBar->getDoubleValues();
    QVector<QPair<QWidget *, double>> valueByRow;
    valueByRow.reserve(m_rows.size());
    for (int i = 0; i < m_rows.size(); ++i)
        valueByRow.append({m_rows[i].container, previousValues.value(i)});

    syncRowsWithListItems();

    QVector<double> reorderedValues;
    reorderedValues.reserve(m_rows.size());
    for (const auto &row : std::as_const(m_rows)) {
        double value = 0;
        for (const auto &[container, rowValue] : valueByRow) {
            if (container == row.container) {
                value = rowValue;
                break;
            }
        }
        reorderedValues.append(value);
    }

    setRowsValues(reorderedValues);
    refreshComboBoxItems();
    updateBarLabelsAndColors();
}

void SpeakerMixList::onSpeakerTypeChanged(int index) {
    const int rowIndex = findRowIndexBySender(sender());
    if (rowIndex == -1)
        return;

    const QString newName =
        m_rows[rowIndex].speakerComboBox->itemData(index, Qt::UserRole).toString();
    const QString oldName = m_rows[rowIndex].speakerName;

    if (newName.isEmpty() || newName == oldName)
        return;
    const int existingRowIndex = findRowIndexBySpeaker(newName);
    if (existingRowIndex != -1 && existingRowIndex != rowIndex) {
        refreshComboBoxItems();
        return;
    }

    m_rows[rowIndex].speakerName = newName;
    updateRowColor(m_rows[rowIndex]);
    updateBarLabelsAndColors();
    refreshComboBoxItems();

    emit speakerChanged(oldName, newName);
}

void SpeakerMixList::syncRowsFromBar(const QVector<double> &values) {
    double cumulative = 0;
    for (int i = 0; i < m_rows.size() && i < values.size(); ++i) {
        m_rows[i].positionLabel->setText(QString::number(qRound(cumulative)) + "%");
        cumulative += values[i];
    }
    updateBarLabelsAndColors();
}

void SpeakerMixList::setRowsValues(const QVector<double> &values) {
    m_mixBar->setDoubleValues(values);

    double cumulative = 0;
    for (int i = 0; i < m_rows.size() && i < values.size(); ++i) {
        m_rows[i].positionLabel->setText(QString::number(qRound(cumulative)) + "%");
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
        if (m_rows[i].speakerComboBox == object) {
            return i;
        }
    }
    return -1;
}

int SpeakerMixList::findRowIndexBySpeaker(const QString &speakerName) const {
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].speakerName == speakerName)
            return i;
    }
    return -1;
}

void SpeakerMixList::refreshComboBoxItems() {
    QSet<QString> usedNames;
    for (const auto &row : std::as_const(m_rows))
        usedNames.insert(row.speakerName);

    for (auto &row : m_rows) {
        const QSignalBlocker blocker(row.speakerComboBox);
        row.speakerComboBox->clear();

        int selectIndex = -1;
        int popupWidth = row.speakerComboBox->width();
        const QFontMetrics fontMetrics(row.speakerComboBox->view()->font());

        for (int i = 0; i < m_speakerTypes.size(); ++i) {
            const QString &name = m_speakerTypes[i];
            const QString displayName = speakerDisplayName(name);
            const bool isCurrentRow = (name == row.speakerName);
            const bool isUsedByOther = (!isCurrentRow && usedNames.contains(name));

            const QString displayText =
                isUsedByOther ? displayName + QString::fromUtf8("（已使用）") : displayName;

            row.speakerComboBox->addItem(displayText);
            row.speakerComboBox->setItemData(i, name, Qt::UserRole);
            popupWidth = std::max(popupWidth, fontMetrics.horizontalAdvance(displayText) + 36);

            if (isUsedByOther) {
                row.speakerComboBox->setItemData(i, QColor(255, 255, 255, 96), Qt::ForegroundRole);
                if (auto *model =
                        qobject_cast<QStandardItemModel *>(row.speakerComboBox->model())) {
                    if (auto *item = model->item(i))
                        item->setEnabled(false);
                }
            }

            if (isCurrentRow)
                selectIndex = i;
        }

        if (selectIndex >= 0)
            row.speakerComboBox->setCurrentIndex(selectIndex);
        row.speakerComboBox->view()->setMinimumWidth(popupWidth);
    }
}

void SpeakerMixList::updateRowColor(RowComponents &row) {
    const int fallbackIndex = m_speakerTypes.indexOf(row.speakerName);
    row.color = SpeakerMixColorResolver::colorsForSpeaker(row.speakerName, m_referenceSpeakers,
                                                          fallbackIndex >= 0 ? fallbackIndex : 0)
                    .accent;
    static_cast<ColorDot *>(row.colorDot)->setColor(row.color);
}

void SpeakerMixList::updateBarLabelsAndColors() {
    QVector<QString> labels;
    labels.reserve(m_rows.size());
    for (const auto &row : std::as_const(m_rows))
        labels.append(speakerDisplayName(row.speakerName));
    m_mixBar->setLabels(labels);
    m_mixBar->setSegmentColors(getColors());
}

QString SpeakerMixList::speakerDisplayName(const QString &speakerName) const {
    const auto displayName = m_speakerDisplayNames.value(speakerName);
    return displayName.isEmpty() ? speakerName : displayName;
}

QVector<QColor> SpeakerMixList::getColors() const {
    QVector<QColor> colors;
    colors.reserve(m_rows.size());
    for (const auto &row : m_rows) {
        colors.append(row.color);
    }
    return colors;
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
        labels.append(row.speakerName);
    }
    return labels;
}

bool SpeakerMixList::eventFilter(QObject *watched, QEvent *event) {
    if (!m_sourceEditingEnabled)
        return QListWidget::eventFilter(watched, event);

    if (auto *handle = qobject_cast<QLabel *>(watched);
        handle && handle->cursor().shape() == Qt::SizeAllCursor) {
        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            auto *viewportEvent =
                new QMouseEvent(event->type(), viewport()->mapFromGlobal(me->globalPosition()),
                                me->globalPosition(), me->button(), me->buttons(), me->modifiers());
            QCoreApplication::sendEvent(viewport(), viewportEvent);
            delete viewportEvent;
            return true;
        }
    }

    return QListWidget::eventFilter(watched, event);
}

void SpeakerMixList::resizeEvent(QResizeEvent *event) {
    QListWidget::resizeEvent(event);
    doItemsLayout();
}
