#include "TwoLevelComboBox.h"
#include <QDebug>
#include <QPainter>
#include <QStyleOptionComboBox>
#include <QStyleOptionToolButton>

TwoLevelComboBox::TwoLevelComboBox(QWidget *parent) : QToolButton(parent) {
    m_mainMenu = new Menu(this);
    setMenu(m_mainMenu);
    setPopupMode(InstantPopup);
    setMinimumHeight(24);
}

TwoLevelComboBox::~TwoLevelComboBox() {
}

void TwoLevelComboBox::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    QStyleOptionToolButton opt;
    opt.initFrom(this);
    opt.features = QStyleOptionToolButton::MenuButtonPopup;
    opt.toolButtonStyle = Qt::ToolButtonTextOnly;
    style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &painter, this);
    
    QRect textRect = opt.rect;
    textRect.adjust(8, 0, -28, 0);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, currentText().isEmpty() ? tr("Please select") : currentText());
}

QSize TwoLevelComboBox::sizeHint() const {
    QStyleOptionToolButton opt;
    opt.initFrom(this);
    opt.features = QStyleOptionToolButton::MenuButtonPopup;
    opt.toolButtonStyle = Qt::ToolButtonTextOnly;
    const QFontMetrics fm = fontMetrics();
    QString text = currentText().isEmpty() ? tr("Please select") : currentText();
    QSize textSize = fm.size(Qt::TextSingleLine, text);
    textSize.setWidth(textSize.width() + 36);
    
    // Use default height, will be overridden by stylesheet or explicit size setting
    textSize.setHeight(QToolButton::sizeHint().height());
    
    return style()->sizeFromContents(QStyle::CT_ToolButton, &opt, textSize, this);
}

QSize TwoLevelComboBox::minimumSizeHint() const {
    return sizeHint();
}

void TwoLevelComboBox::addGroup(const QString &groupName) const {
    const auto groupMenu = new Menu(groupName, m_mainMenu);
    m_mainMenu->addMenu(groupMenu);
}

void TwoLevelComboBox::addItem(const QString &itemText, const SingerInfo &singer,
                               const SpeakerInfo &spk) {
    addItemInternal(itemText, singer, spk, m_mainMenu);
}

void TwoLevelComboBox::addItemToGroup(const QString &groupName, const QString &itemText,
                                      const SingerInfo &singer, const SpeakerInfo &spk) {
    Menu *targetMenu = createGroupMenu(groupName);
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

void TwoLevelComboBox::setItems(const QList<PackageInfo> &packages) {
    const bool inheritWasSelected = m_inheritWasSelected;
    const SingerInfo prevSinger = inheritWasSelected ? SingerInfo() : m_currentItem.singer;
    const SpeakerInfo prevSpeaker = inheritWasSelected ? SpeakerInfo() : m_currentItem.speaker;

    clear();

    if (m_showInheritItem) {
        // "Follow Track" inherit item — singer/speaker populated later via setCurrentData
        const auto action = new QAction(tr("Follow Track"), this);
        ComboBoxItemData inheritData;
        inheritData.text = tr("Follow Track");
        inheritData.isInheritItem = true;
        inheritData.action = action;

        const QVariant variant = QVariant::fromValue(inheritData);
        action->setProperty(m_propertyName.toUtf8(), variant);
        m_mainMenu->addAction(action);
        m_itemDataList.append(inheritData);
        connect(action, &QAction::triggered, this, [this, action] { onActionTriggered(action); });

        m_mainMenu->addSeparator();
    }

    addItem(tr("(No singer)"), {}, {});
    for (const auto &package : std::as_const(packages)) {
        const auto singers = package.singers();
        for (const auto &singer : singers) {
            QString singerText = singer.name();
            if (singer.speakers().size() == 1) {
                const auto spk = singer.speakers().first();
                addItem(spk.id(), singer, spk);
                continue;
            }
            if (singer.speakers().isEmpty()) {
                addItem(singerText, singer, {});
                continue;
            }
            addGroup(singerText);
            for (const auto &spk : singer.speakers())
                addItemToGroup(singerText, spk.name(), singer, spk);
        }
    }

    // Restore previous selection
    if (inheritWasSelected && m_showInheritItem) {
        // Re-select inhert item — caller will update singer/speaker via setCurrentData(preferInherit=true)
        if (!m_itemDataList.isEmpty() && m_itemDataList.first().isInheritItem) {
            m_currentItem = m_itemDataList.first();
        }
        m_inheritWasSelected = true;
    } else if (!prevSinger.isEmpty() || !prevSpeaker.isEmpty()) {
        setCurrentData(prevSinger, prevSpeaker);
    }
    // else: stays on "(No singer)" (already set by addItem in clear+rebuild)

    // If nothing was selected, default to first item
    if (m_currentItem.text.isEmpty() && m_currentItem.singer.isEmpty() && !m_currentItem.isInheritItem) {
        if (!m_itemDataList.isEmpty()) {
            m_currentItem = m_itemDataList.first();
        }
    }
    updateDisplayText();
}

QString TwoLevelComboBox::currentText() const {
    const QString singerName = m_currentItem.singer.name();
    const QString speakerName = m_currentItem.speaker.name();
    QString effectiveText;
    if (singerName.isEmpty()) {
        effectiveText = m_currentItem.text;
    } else if (speakerName.isEmpty()) {
        effectiveText = singerName;
    } else {
        effectiveText = singerName + " / " + speakerName;
    }
    if (m_currentItem.isInheritItem && effectiveText != m_currentItem.text) {
        return tr("Follow Track") + " (" + effectiveText + ")";
    }
    return effectiveText;
}

SingerInfo TwoLevelComboBox::currentSinger() const {
    return m_currentItem.singer;
}

SpeakerInfo TwoLevelComboBox::currentSpeaker() const {
    return m_currentItem.speaker;
}

void TwoLevelComboBox::setCurrentData(const SingerInfo &singer, const SpeakerInfo &speaker,
                                    const bool preferInherit) {
    if (preferInherit && m_showInheritItem) {
        // Find the inherit item and update its display singer/speaker
        for (auto &itemData : m_itemDataList) {
            if (itemData.isInheritItem) {
                itemData.singer = singer;
                itemData.speaker = speaker;
                m_currentItem = itemData;
                m_inheritWasSelected = true;
                updateDisplayText();
                return;
            }
        }
    }
    m_inheritWasSelected = false;
    for (const auto &itemData : m_itemDataList) {
        if (itemData.singer == singer && itemData.speaker == speaker) {
            m_currentItem = itemData;
            updateDisplayText();
            return;
        }
    }
}

bool TwoLevelComboBox::isInheritSelected() const {
    return m_currentItem.isInheritItem;
}

void TwoLevelComboBox::setShowInheritItem(const bool show) {
    m_showInheritItem = show;
}

void TwoLevelComboBox::onActionTriggered(const QAction *action) {
    const QVariant variant = action->property(m_propertyName.toUtf8());
    if (variant.canConvert<ComboBoxItemData>()) {
        const auto itemData = variant.value<ComboBoxItemData>();

        if (!itemData.isInheritItem) {
            // When selecting a concrete singer, emit with the selected singer/speaker data
            m_inheritWasSelected = false;
        }
        m_currentItem = itemData;
        updateDisplayText();

        emit currentDataChanged();
        emit currentTextChanged(m_currentItem.text);
    }
}

void TwoLevelComboBox::updateDisplayText() {
    update();
}

Menu *TwoLevelComboBox::createGroupMenu(const QString &groupName) const {
    QList<QAction *> actions = m_mainMenu->actions();
    for (const QAction *action : actions) {
        if (action->menu() && action->text() == groupName) {
            return qobject_cast<Menu *>(action->menu());
        }
    }
    const auto groupMenu = new Menu(groupName, m_mainMenu);
    m_mainMenu->addMenu(groupMenu);
    return groupMenu;
}
