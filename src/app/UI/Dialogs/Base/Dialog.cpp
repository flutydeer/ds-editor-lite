#include "Dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWKWidgets/widgetwindowagent.h>
#include <QWindow>

#include "DialogTitleBar.h"
#include "Model/AppOptions/AppOptions.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/PlatformDialogButtonBox.h>
#include <lite/GUI/Controls/SystemWindowButton.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Utils/WindowFrameUtils.h>

QPointer<QWidget> Dialog::m_globalParent;

DialogHeader::DialogHeader(QWidget *parent) : QWidget(parent) {
    m_lbTitle = new QLabel;
    m_lbTitle->setObjectName("DialogTitleLabel");
    m_lbTitle->setWordWrap(true);
    m_lbTitle->setVisible(false);

    m_lbMessage = new QLabel;
    m_lbMessage->setObjectName("DialogMessageLabel");
    // Let the layout use height-for-width instead of clipping a single long line.
    m_lbMessage->setWordWrap(true);
    m_lbMessage->setVisible(false);

    m_mainLayout = new QVBoxLayout;
    m_mainLayout->addWidget(m_lbTitle);
    m_mainLayout->addWidget(m_lbMessage);
    m_mainLayout->setContentsMargins(12, 12, 12, 0);
    m_mainLayout->setSpacing(8);

    setLayout(m_mainLayout);
}

DialogHeader::~DialogHeader() {
    delete m_mainLayout;
    delete m_lbTitle;
    delete m_lbMessage;
}

void DialogHeader::setTitle(const QString &title) const {
    m_lbTitle->setVisible(true);
    m_lbTitle->setText(title);
}

void DialogHeader::setMessage(const QString &msg) const {
    m_lbMessage->setVisible(true);
    m_lbMessage->setText(msg);
}

DialogButtonBar::DialogButtonBar(QWidget *parent) : QWidget(parent) {
    setObjectName("DialogButtonBar");
    setAttribute(Qt::WA_StyledBackground);

    m_mainLayout = new QHBoxLayout;
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins({12, 12, 12, 12});
    m_buttonBox = new PlatformDialogButtonBox(this);
    m_mainLayout->addWidget(m_buttonBox);

    setLayout(m_mainLayout);
    setContentsMargins({});
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

DialogButtonBar::~DialogButtonBar() {
    delete m_mainLayout;
}

void DialogButtonBar::addButton(Button *button, QDialogButtonBox::ButtonRole role) const {
    m_buttonBox->addButton(button, role);
}

void DialogButtonBar::reset() {
    // clear() deletes the buttons. They are retained by Dialog and reinserted.
    for (auto *button : m_buttonBox->buttons()) {
        m_buttonBox->removeButton(button);
        button->setParent(this);
        button->hide();
    }
}

QDialogButtonBox *DialogButtonBar::buttonBox() const {
    return m_buttonBox;
}

Dialog::Dialog(QWidget *parent, const Qt::WindowFlags f)
    : QDialog(parent ? parent : m_globalParent.data(), f) {
    m_header = new DialogHeader;
    m_header->setVisible(false);

    m_body = new QWidget;
    m_body->setContentsMargins(12, 12, 12, 0);

    m_mainLayout = new QVBoxLayout;
    m_mainLayout->addWidget(m_header);
    m_mainLayout->addWidget(m_body);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    setContentsMargins(0, 0, 0, 0);
    setLayout(m_mainLayout);
    setMinimumWidth(320);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    const auto useNativeFrame = appOptions->appearance()->effectiveUseNativeFrame();
    if (!useNativeFrame) {
#ifdef Q_OS_MAC
        // Match the main window before the agent creates the native window.
        // Cocoa reports the title bar as a safe area even after expanding the
        // client view; QWidget must not reserve that row above our title bar.
        setWindowFlag(Qt::ExpandedClientAreaHint, true);
        setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
        setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
        // macOS keeps its traffic lights, so the title bar only has to give them
        // a slot; drawing a Windows-style close button next to them duplicated
        // what the platform already does.
        constexpr bool trafficLightArea = true;
#else
        constexpr bool trafficLightArea = false;
#endif
        m_dialogTitleBar = new DialogTitleBar(this, trafficLightArea);
        m_mainLayout->insertWidget(0, m_dialogTitleBar);

        m_agent = new QWK::WidgetWindowAgent(this);
        m_agent->setup(this);
        m_agent->setTitleBar(m_dialogTitleBar);
#ifdef Q_OS_MAC
        m_agent->setSystemButtonArea(m_dialogTitleBar->trafficLightArea());
        // Same early merge as the main window: setup() already created the native
        // window, and the mask has to be in place before the content is laid out.
        WindowFrameUtils::mergeTitleBarIntoContent(this);
#else
        m_agent->setSystemButton(QWK::WindowAgentBase::Close, m_dialogTitleBar->closeButton());
        connect(m_dialogTitleBar, &DialogTitleBar::closeTriggered, this, &QDialog::reject);
#endif
    }

    ThemeManager::instance()->addWindow(this);
}

Dialog::~Dialog() {
    ThemeManager::instance()->removeWindow(this);
    // Let QWidget tear down the button bar after QDialog clears focus. Deleting
    // it here can send an auto-default button's focus event into a partially
    // destroyed child tree.
}

bool Dialog::event(QEvent *event) {
    // Keep the right-edge workaround filter attached to the platform window,
    // re-installing it whenever the native window is recreated (e.g. by
    // setWindowFlag calls).
    if (event->type() == QEvent::WinIdChange) {
        if (auto *window = windowHandle())
            window->installEventFilter(this);
    } else if (event->type() == QEvent::Show && m_dialogTitleBar) {
        // Same reclaim as the main window. Show alone is too early: the NSView
        // has no NSWindow yet at that point, so repeat it once the window is up.
        QTimer::singleShot(0, this, [this] { WindowFrameUtils::mergeTitleBarIntoContent(this); });
    } else if (event->type() == QEvent::WindowActivate && m_dialogTitleBar) {
        WindowFrameUtils::mergeTitleBarIntoContent(this);
    }
    return QDialog::event(event);
}

bool Dialog::eventFilter(QObject *watched, QEvent *event) {
    if (m_dialogTitleBar && watched == windowHandle()) {
        switch (event->type()) {
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick: {
                const auto *mouseEvent = static_cast<QMouseEvent *>(event);
                const QPointF pos = mouseEvent->position();
                // Same right-edge gap workaround as MainWindow::eventFilter; see
                // docs/design/title-bar-edge-deadzone-fix-design.md.
                if (pos.x() >= width() - 1.0 && m_dialogTitleBar->systemButtonAt(pos)) {
                    const QPointF fixedPos(width() - 1.5, pos.y());
                    QMouseEvent fixed(mouseEvent->type(), fixedPos, fixedPos,
                                      mouseEvent->globalPosition(), mouseEvent->button(),
                                      mouseEvent->buttons(), mouseEvent->modifiers(),
                                      mouseEvent->source(), mouseEvent->pointingDevice());
                    fixed.setTimestamp(mouseEvent->timestamp());
                    QCoreApplication::sendEvent(watched, &fixed);
                    return true;
                }
                break;
            }
            default:
                break;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void Dialog::setTitle(const QString &title) const {
    m_header->setVisible(true);
    m_header->setTitle(title);
}

void Dialog::setMessage(const QString &msg) const {
    m_header->setVisible(true);
    m_header->setMessage(msg);
}

void Dialog::setPositiveButton(Button *button, QDialogButtonBox::ButtonRole role) {
    m_positiveButton = button;
    m_positiveRole = role;
    setButton();
}

void Dialog::setNegativeButton(Button *button, QDialogButtonBox::ButtonRole role) {
    m_negativeButton = button;
    m_negativeRole = role;
    setButton();
}

void Dialog::setNeutralButton(Button *button, QDialogButtonBox::ButtonRole role) {
    m_neutralButton = button;
    m_neutralRole = role;
    setButton();
}

QWidget *Dialog::body() const {
    return m_body;
}

DialogButtonBar *Dialog::buttonBar() const {
    if (!m_buttonBar)
        const_cast<Dialog *>(this)->createButtonBar();
    return m_buttonBar;
}

QWidget *Dialog::globalParent() {
    return m_globalParent;
}

QWK::WidgetWindowAgent *Dialog::windowAgent() const {
    return m_agent;
}

void Dialog::setGlobalContext(QWidget *parent) {
    m_globalParent = parent;
}

void Dialog::createButtonBar() {
    m_buttonBar = new DialogButtonBar(this);
    // The 12px gap above the button bar only applies when buttons exist;
    // button-less dialogs (e.g. the options dialog) get no dead space at the
    // bottom.
    m_mainLayout->addSpacing(12);
    m_mainLayout->addWidget(m_buttonBar);
}

void Dialog::setButton() {
    if (!m_buttonBar)
        createButtonBar();
    m_buttonBar->reset();

    if (m_positiveButton) {
        m_buttonBar->addButton(m_positiveButton, m_positiveRole);
        m_positiveButton->show();
        m_positiveButton->setDefault(true);
    }

    if (m_negativeButton) {
        m_buttonBar->addButton(m_negativeButton, m_negativeRole);
        m_negativeButton->show();
    }

    if (m_neutralButton) {
        m_buttonBar->addButton(m_neutralButton, m_neutralRole);
        m_neutralButton->show();
    }
}
