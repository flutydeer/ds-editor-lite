//
// Created by fluty on 24-2-19.
//

#include "Dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

#include "UI/Controls/Button.h"
#include "UI/Utils/ThemeManager.h"
#include "Utils/WindowFrameUtils.h"

QWidget *Dialog::m_globalParent = nullptr;

DialogHeader::DialogHeader(QWidget *parent) : QWidget(parent) {
    m_lbTitle = new QLabel;
    m_lbTitle->setObjectName("DialogTitleLabel");
    m_lbTitle->setVisible(false);

    m_lbMessage = new QLabel;
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
    m_mainLayout->addStretch(1);

    setLayout(m_mainLayout);
    setContentsMargins({});
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

DialogButtonBar::~DialogButtonBar() {
    delete m_mainLayout;
}

void DialogButtonBar::addButton(Button *button) const {
    m_mainLayout->addWidget(button);
}

void DialogButtonBar::reset() const {
    for (const auto child : m_mainLayout->children())
        if (const auto button = dynamic_cast<Button *>(child))
            m_mainLayout->removeWidget(button);
}

Dialog::Dialog(QWidget *parent, const Qt::WindowFlags f)
    : QDialog(parent ? parent : m_globalParent, f) {
    m_header = new DialogHeader;
    m_header->setVisible(false);
    // setTitle("要删除此乐器吗？");
    // setMessage("由于遇到严重的错误，应用程序需要重新启动。请联系开发者并提供应用程序的日志。");

    m_body = new QWidget;
    m_body->setContentsMargins(12, 12, 12, 0);

    m_mainLayout = new QVBoxLayout;
    m_mainLayout->addWidget(m_header);
    m_mainLayout->addWidget(m_body);
    m_mainLayout->addSpacing(12);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    setContentsMargins(0, 0, 0, 0);
    setLayout(m_mainLayout);
    setMinimumWidth(320);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    ThemeManager::instance()->addWindow(this);
}

Dialog::~Dialog() {
    ThemeManager::instance()->removeWindow(this);
    delete m_buttonBar;
}

void Dialog::setTitle(const QString &title) const {
    m_header->setVisible(true);
    m_header->setTitle(title);
}

void Dialog::setMessage(const QString &msg) const {
    m_header->setVisible(true);
    m_header->setMessage(msg);
}

void Dialog::setPositiveButton(Button *button) {
    m_positiveButton = button;
    setButton();
}

void Dialog::setNegativeButton(Button *button) {
    m_negativeButton = button;
    setButton();
}

void Dialog::setNeutralButton(Button *button) {
    m_neutralButton = button;
    setButton();
}

QWidget *Dialog::body() const {
    return m_body;
}

DialogButtonBar *Dialog::buttonBar() const {
    return m_buttonBar;
}

QWidget *Dialog::globalParent() {
    return m_globalParent;
}

void Dialog::setGlobalContext(QWidget *parent) {
    m_globalParent = parent;
}

void Dialog::createButtonBar() {
    m_buttonBar = new DialogButtonBar(this);
    m_mainLayout->addWidget(m_buttonBar);
}

void Dialog::setButton() {
    if (!m_buttonBar)
        createButtonBar();
    m_buttonBar->reset();

    if (m_positiveButton) {
        m_buttonBar->addButton(m_positiveButton);
        m_positiveButton->setDefault(true);
    }

    if (m_negativeButton)
        m_buttonBar->addButton(m_negativeButton);

    if (m_neutralButton)
        m_buttonBar->addButton(m_neutralButton);
}