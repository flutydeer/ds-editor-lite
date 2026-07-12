#include "TwoLevelComboBox.h"

#include "UI/Utils/IconUtils.h"

#include <QDebug>
#include <QIcon>
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
    // 只让 Qt style 绘制按钮背景；文本和箭头由控件自绘，确保 QSS color 能统一生效。
    opt.features = QStyleOptionToolButton::None;
    opt.toolButtonStyle = Qt::ToolButtonTextOnly;
    style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &painter, this);

    const QColor textColor = opt.palette.color(isEnabled() ? QPalette::Active : QPalette::Disabled,
                                               QPalette::ButtonText);
    QRect textRect = opt.rect;
    textRect.adjust(8, 0, -28, 0);
    painter.setPen(textColor);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                     currentText().isEmpty() ? tr("Please select") : currentText());

    constexpr int menuButtonWidth = 28;
    // 箭头沿用项目的 SVG 染色工具，并用 QRectF 计算位置，避免高 DPI 缩放下整数坐标
    // 造成轻微偏移。ComboBox 如需自绘箭头，应复用同样的 IconUtils + QRectF 方式。
    const QRectF controlRect(QPointF(opt.rect.topLeft()), QSizeF(opt.rect.size()));
    const QSizeF arrowSize(16, 16);
    const QRectF arrowRect(controlRect.left() + controlRect.width() - menuButtonWidth +
                               (menuButtonWidth - arrowSize.width()) / 2.0,
                           controlRect.top() + (controlRect.height() - arrowSize.height()) / 2.0,
                           arrowSize.width(), arrowSize.height());
    const auto arrowIcon = IconUtils::createTintedSvgIcon(":svg/icons/chevron_down_16_regular.svg",
                                                          arrowSize.toSize(), textColor, textColor);
    const auto arrowPixmap =
        arrowIcon.pixmap(arrowSize.toSize(), isEnabled() ? QIcon::Normal : QIcon::Disabled);
    painter.drawPixmap(arrowRect.topLeft(), arrowPixmap);
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

    if (targetMenu->menuAction()->property(m_singerIdentifierPropertyName.toUtf8()).isNull())
        targetMenu->menuAction()->setProperty(m_singerIdentifierPropertyName.toUtf8(),
                                              QVariant::fromValue(singer.identifier()));
    addItemInternal(itemText, singer, spk, targetMenu);
}

void TwoLevelComboBox::addItemInternal(const QString &itemText, const SingerInfo &singer,
                                       const SpeakerInfo &spk, QMenu *parentMenu) {
    auto action = new QAction(itemText, this);
    action->setCheckable(true);

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
    updateActionCheckStates();
}

void TwoLevelComboBox::clear() {
    m_mainMenu->clear();
    m_itemDataList.clear();
    m_currentItem = ComboBoxItemData();
    m_checkedInjectedAction = nullptr;
    m_suppressCurrentActionCheck = false;
    m_displayTextOverride.clear();
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
            const auto groupMenu = createGroupMenu(singerText, singer.identifier());
            for (const auto &spk : singer.speakers())
                addItemInternal(spk.name(), singer, spk, groupMenu);
        }
    }

    // Restore previous selection
    if (inheritWasSelected && m_showInheritItem) {
        // Re-select inhert item — caller will update singer/speaker via
        if (!m_itemDataList.isEmpty() && m_itemDataList.first().isInheritItem) {
            m_currentItem = m_itemDataList.first();
        }
        m_inheritWasSelected = true;
    } else if (!prevSinger.isEmpty() || !prevSpeaker.isEmpty()) {
        setCurrentData(prevSinger, prevSpeaker);
    }

    // If nothing was selected, default to first item
    if (m_currentItem.text.isEmpty() && m_currentItem.singer.isEmpty() &&
        !m_currentItem.isInheritItem) {
        if (!m_itemDataList.isEmpty()) {
            m_currentItem = m_itemDataList.first();
        }
    }
    m_loadingText.clear();
    updateDisplayText();
    updateActionCheckStates();
    emit itemsPopulated();
}

QString TwoLevelComboBox::currentText() const {
    if (!m_loadingText.isEmpty())
        return m_loadingText;
    if (!m_displayTextOverride.isEmpty()) {
        if (m_currentItem.isInheritItem)
            return tr("Follow Track") + " (" + m_displayTextOverride + ")";
        return m_displayTextOverride;
    }
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
    m_displayTextOverride.clear();
    m_checkedInjectedAction = nullptr;
    m_suppressCurrentActionCheck = false;
    if (preferInherit && m_showInheritItem) {
        // Find the inherit item and update its display singer/speaker
        for (auto &itemData : m_itemDataList) {
            if (itemData.isInheritItem) {
                itemData.singer = singer;
                itemData.speaker = speaker;
                m_currentItem = itemData;
                m_inheritWasSelected = true;
                updateDisplayText();
                updateActionCheckStates();
                return;
            }
        }
    }
    m_inheritWasSelected = false;
    for (const auto &itemData : m_itemDataList) {
        if (itemData.singer == singer && itemData.speaker == speaker) {
            m_currentItem = itemData;
            updateDisplayText();
            updateActionCheckStates();
            return;
        }
    }
}

void TwoLevelComboBox::setDisplayTextOverride(const QString &text) {
    m_displayTextOverride = text;
    m_suppressCurrentActionCheck = !text.isEmpty();
    updateDisplayText();
    updateActionCheckStates();
}

void TwoLevelComboBox::clearDisplayTextOverride() {
    m_displayTextOverride.clear();
    m_suppressCurrentActionCheck = false;
    updateDisplayText();
    updateActionCheckStates();
}

void TwoLevelComboBox::setCheckedInjectedAction(QAction *action) {
    m_checkedInjectedAction = action;
    updateActionCheckStates();
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
        m_displayTextOverride.clear();
        m_checkedInjectedAction = nullptr;
        m_suppressCurrentActionCheck = false;
        updateDisplayText();
        updateActionCheckStates();

        emit currentDataChanged();
        emit currentTextChanged(m_currentItem.text);
    }
}

void TwoLevelComboBox::updateDisplayText() {
    update();
}

void TwoLevelComboBox::updateActionCheckStates() const {
    for (const auto &itemData : m_itemDataList) {
        if (!itemData.action)
            continue;
        itemData.action->setCheckable(true);
        itemData.action->setChecked(!m_suppressCurrentActionCheck && !m_checkedInjectedAction &&
                                    itemData.action == m_currentItem.action);
    }

    for (const auto action : m_mainMenu->actions()) {
        if (!action->menu())
            continue;
        for (const auto menuAction : action->menu()->actions()) {
            if (!menuAction->property(m_injectedPropertyName.toUtf8()).toBool())
                continue;
            menuAction->setCheckable(true);
            menuAction->setChecked(menuAction == m_checkedInjectedAction);
        }
    }
}

void TwoLevelComboBox::setLoadingText(const QString &text) {
    m_loadingText = text;
    update();
}

Menu *TwoLevelComboBox::mainMenu() const {
    return m_mainMenu;
}

Menu *TwoLevelComboBox::groupMenuForSinger(const SingerInfo &singer) const {
    const auto identifier = singer.identifier();
    for (const auto action : m_mainMenu->actions()) {
        if (!action->menu())
            continue;
        const auto variant = action->property(m_singerIdentifierPropertyName.toUtf8());
        if (variant.canConvert<SingerIdentifier>() &&
            variant.value<SingerIdentifier>() == identifier) {
            return qobject_cast<Menu *>(action->menu());
        }
    }
    return nullptr;
}

void TwoLevelComboBox::clearInjectedActions() const {
    const_cast<TwoLevelComboBox *>(this)->m_checkedInjectedAction = nullptr;
    const auto actions = m_mainMenu->actions();
    for (const auto action : actions) {
        if (!action->menu())
            continue;
        const auto menuActions = action->menu()->actions();
        for (const auto menuAction : menuActions) {
            if (menuAction->property(m_injectedPropertyName.toUtf8()).toBool()) {
                action->menu()->removeAction(menuAction);
                menuAction->deleteLater();
            }
        }
    }
}

QAction *TwoLevelComboBox::addInjectedActionToSinger(const SingerInfo &singer,
                                                     const QString &text) const {
    const auto groupMenu = groupMenuForSinger(singer);
    if (!groupMenu)
        return nullptr;

    const auto action = new QAction(text, const_cast<TwoLevelComboBox *>(this));
    action->setCheckable(true);
    action->setProperty(m_injectedPropertyName.toUtf8(), true);
    groupMenu->addAction(action);
    updateActionCheckStates();
    return action;
}

QAction *TwoLevelComboBox::addInjectedSeparatorToSinger(const SingerInfo &singer) const {
    const auto groupMenu = groupMenuForSinger(singer);
    if (!groupMenu)
        return nullptr;

    const auto action = groupMenu->addSeparator();
    action->setProperty(m_injectedPropertyName.toUtf8(), true);
    return action;
}

Menu *TwoLevelComboBox::createGroupMenu(const QString &groupName) const {
    return createGroupMenu(groupName, {});
}

Menu *TwoLevelComboBox::createGroupMenu(const QString &groupName,
                                        const SingerIdentifier &identifier) const {
    QList<QAction *> actions = m_mainMenu->actions();
    for (const QAction *action : actions) {
        if (!action->menu())
            continue;
        const auto variant = action->property(m_singerIdentifierPropertyName.toUtf8());
        if (!identifier.isEmpty() && variant.canConvert<SingerIdentifier>() &&
            variant.value<SingerIdentifier>() == identifier) {
            return qobject_cast<Menu *>(action->menu());
        }
        if (identifier.isEmpty() && action->text() == groupName) {
            return qobject_cast<Menu *>(action->menu());
        }
    }
    const auto groupMenu = new Menu(groupName, m_mainMenu);
    m_mainMenu->addMenu(groupMenu);
    if (!identifier.isEmpty())
        groupMenu->menuAction()->setProperty(m_singerIdentifierPropertyName.toUtf8(),
                                             QVariant::fromValue(identifier));
    return groupMenu;
}
