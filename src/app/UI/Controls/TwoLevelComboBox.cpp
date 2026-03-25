#include "TwoLevelComboBox.h"
#include <QDebug>
#include <QPainter>
#include <QStyleOptionComboBox>

TwoLevelComboBox::TwoLevelComboBox(QWidget *parent) : QComboBox(parent) {
    m_mainMenu = new CMenu(this);
}

TwoLevelComboBox::~TwoLevelComboBox() {
}

void TwoLevelComboBox::showPopup() {
    QComboBox::hidePopup();
    if (m_mainMenu && !m_mainMenu->isEmpty()) {
        QPoint pos = mapToGlobal(QPoint(0, height()));
        m_mainMenu->popup(pos);
    }
}

void TwoLevelComboBox::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    QStyleOptionComboBox opt;
    opt.initFrom(this);
    opt.currentText = m_currentItem.text.isEmpty() ? tr("Please select") : m_currentItem.text;
    opt.editable = false;
    style()->drawComplexControl(QStyle::CC_ComboBox, &opt, &painter, this);
    style()->drawControl(QStyle::CE_ComboBoxLabel, &opt, &painter, this);
}

void TwoLevelComboBox::addGroup(const QString &groupName) const {
    const auto groupMenu = new CMenu(groupName, m_mainMenu);
    m_mainMenu->addMenu(groupMenu);
}

void TwoLevelComboBox::addItem(const QString &itemText, const SingerInfo &singer,
                               const SpeakerInfo &spk) {
    addItemInternal(itemText, singer, spk, m_mainMenu);
}

void TwoLevelComboBox::addItemToGroup(const QString &groupName, const QString &itemText,
                                      const SingerInfo &singer, const SpeakerInfo &spk) {
    CMenu *targetMenu = createGroupMenu(groupName);
    if (!targetMenu)
        return;

    addItemInternal(itemText, singer, spk, targetMenu);
}

void TwoLevelComboBox::addItemInternal(const QString &itemText, const SingerInfo &singer,
                                       const SpeakerInfo &spk, QMenu *parentMenu) {
    auto action = new QAction(itemText, this);

    ComboBoxItemData itemData;
    itemData.text = itemText;
    itemData.singer = singer;
    itemData.speaker = spk;
    itemData.action = action;

    const QVariant variant = QVariant::fromValue(itemData);
    action->setProperty(m_propertyName.toUtf8(), variant);

    m_itemDataList.append(itemData);

    parentMenu->addAction(action);

    connect(action, &QAction::triggered, this, [this, action] { this->onActionTriggered(action); });

    if (m_itemDataList.size() == 1) {
        m_currentItem = itemData;
        updateDisplayText();
    }
}

void TwoLevelComboBox::clear() {
    m_mainMenu->clear();
    m_itemDataList.clear();
    m_currentItem = ComboBoxItemData();
    update();
}

QString TwoLevelComboBox::currentText() const {
    return m_currentItem.text;
}

SingerInfo TwoLevelComboBox::currentSinger() const {
    return m_currentItem.singer;
}

SpeakerInfo TwoLevelComboBox::currentSpeaker() const {
    return m_currentItem.speaker;
}

void TwoLevelComboBox::onActionTriggered(const QAction *action) {
    const QVariant variant = action->property(m_propertyName.toUtf8());
    if (variant.canConvert<ComboBoxItemData>()) {
        const auto itemData = variant.value<ComboBoxItemData>();

        m_currentItem = itemData;
        updateDisplayText();

        emit currentDataChanged();
        emit currentTextChanged(m_currentItem.text);
    }
}

void TwoLevelComboBox::updateDisplayText() {
    update();
}

CMenu *TwoLevelComboBox::createGroupMenu(const QString &groupName) const {
    QList<QAction *> actions = m_mainMenu->actions();
    for (const QAction *action : actions) {
        if (action->menu() && action->text() == groupName) {
            return reinterpret_cast<CMenu *>(action->menu());
        }
    }
    const auto groupMenu = new CMenu(groupName, m_mainMenu);
    m_mainMenu->addMenu(groupMenu);
    return groupMenu;
}
