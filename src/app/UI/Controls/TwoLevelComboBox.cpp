#include "TwoLevelComboBox.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QToolButton>

TwoLevelComboBox::TwoLevelComboBox(QWidget *parent) : QWidget(parent) {
    setupUi();
}

TwoLevelComboBox::~TwoLevelComboBox() {
}

void TwoLevelComboBox::setupUi() {
    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_toolButton = new QToolButton(this);
    m_mainMenu = new CMenu(this);

    m_toolButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolButton->setMenu(m_mainMenu);
    m_toolButton->setText(tr("请选择"));
    layout->addWidget(m_toolButton);
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
        updateButtonText();
    }
}

void TwoLevelComboBox::clear() {
    m_mainMenu->clear();
    m_itemDataList.clear();
    m_currentItem = ComboBoxItemData();
    m_toolButton->setText(tr("请选择"));
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
        updateButtonText();

        emit currentDataChanged();
        emit currentTextChanged();
    }
}

void TwoLevelComboBox::updateButtonText() const {
    if (!m_currentItem.text.isEmpty()) {
        m_toolButton->setText(m_currentItem.text);
    }
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