#include "SegmentedTabBar.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>

SegmentedTabBar::SegmentedTabBar(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins({});
    m_layout->setSpacing(1);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);
    connect(m_buttonGroup, &QButtonGroup::idClicked, this, &SegmentedTabBar::setCurrentIndex);
}

int SegmentedTabBar::addTab(const QString &text) {
    auto *button = new QPushButton(text, this);
    button->setCheckable(true);
    button->setFocusPolicy(Qt::TabFocus);
    button->setProperty("panelSegment", true);

    m_buttons.append(button);
    m_layout->addWidget(button);
    m_buttonGroup->addButton(button, m_buttons.size() - 1);
    updateSegmentRoles();

    if (m_currentIndex < 0)
        setCurrentIndex(m_buttons.size() - 1);

    return m_buttons.size() - 1;
}

int SegmentedTabBar::count() const {
    return m_buttons.size();
}

QString SegmentedTabBar::tabText(int index) const {
    if (index < 0 || index >= m_buttons.size())
        return {};
    return m_buttons.at(index)->text();
}

void SegmentedTabBar::setTabText(int index, const QString &text) {
    if (index < 0 || index >= m_buttons.size())
        return;
    m_buttons.at(index)->setText(text);
}

int SegmentedTabBar::currentIndex() const {
    return m_currentIndex;
}

void SegmentedTabBar::setCurrentIndex(int index) {
    if (index < 0 || index >= m_buttons.size() || index == m_currentIndex)
        return;

    m_currentIndex = index;
    m_buttons.at(index)->setChecked(true);
    emit currentChanged(index);
}

QRect SegmentedTabBar::tabRect(int index) const {
    if (index < 0 || index >= m_buttons.size())
        return {};
    return m_buttons.at(index)->geometry();
}

void SegmentedTabBar::updateSegmentRoles() {
    const auto size = m_buttons.size();
    for (int i = 0; i < size; ++i) {
        auto *button = m_buttons.at(i);
        QString role;
        if (size == 1)
            role = QStringLiteral("only");
        else if (i == 0)
            role = QStringLiteral("first");
        else if (i == size - 1)
            role = QStringLiteral("last");
        else
            role = QStringLiteral("middle");

        if (button->property("segmentRole") != role) {
            button->setProperty("segmentRole", role);
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }
}
