//
// Created by fluty on 2023/8/30.
//

#include <QGraphicsDropShadowEffect>
#include <QVBoxLayout>
#include <QLabel>

#include "ToolTip.h"

ToolTip::ToolTip(const QString& title, QWidget *parent) : QFrame(parent) {
    m_lbTitle = new QLabel(title);
    m_lbTitle->setStyleSheet("color: #F0F0F0; font-size: 10pt");
    m_lbTitle->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_lbShortcutKey = new QLabel();
    m_lbShortcutKey->setStyleSheet("color: #808080");
    m_lbShortcutKey->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_lbShortcutKey->setVisible(false);

    auto titleShortcutLayout = new QHBoxLayout;
    titleShortcutLayout->addWidget(m_lbTitle);
    titleShortcutLayout->addWidget(m_lbShortcutKey);
    titleShortcutLayout->setContentsMargins({});

    m_messageLayout = new QVBoxLayout;
    m_messageLayout->setContentsMargins({});
    m_messageLayout->setSpacing(0);

    m_cardLayout = new QVBoxLayout;
    m_cardLayout->addLayout(titleShortcutLayout);
    m_cardLayout->addLayout(m_messageLayout);
    //    cardLayout->addWidget(m_teMessage);
    m_cardLayout->setContentsMargins({});

    auto container = new QFrame;
    container->setObjectName("container");
    container->setLayout(m_cardLayout);
    container->setContentsMargins(8, 4, 8, 4);
    container->setStyleSheet("QFrame#container {"
                             "background: #202122; "
                             "border: 1px solid #606060; "
                             "border-radius: 6px; "
                             "font-size: 10pt }");

    auto shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(24);
    shadowEffect->setColor(QColor(0, 0, 0, 32));
    shadowEffect->setOffset(0, 4);
    container->setGraphicsEffect(shadowEffect);

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(container);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    setLayout(mainLayout);

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setWindowOpacity(0);
}

ToolTip::~ToolTip() {
    delete m_lbTitle;
}

QString ToolTip::title() const {
    return m_title;
}

void ToolTip::setTitle(const QString &text) {
    m_title = text;
    m_lbTitle->setText(m_title);
}

QString ToolTip::shortcutKey() const {
    return m_shortcutKey;
}

void ToolTip::setShortcutKey(const QString &text) {
    m_lbShortcutKey->setVisible(true);
    m_shortcutKey = text;
    m_lbShortcutKey->setText(m_shortcutKey);
}

QList<QString> ToolTip::message() const {
    return m_message;
}

void ToolTip::setMessage(const QList<QString> &text) {
    m_message.clear();
    m_message.append(text);
    updateMessage();
}

void ToolTip::appendMessage(const QString &text) {
    m_message.append(text);
    updateMessage();
}

void ToolTip::clearMessage() {
    m_message.clear();
    updateMessage();
}

void ToolTip::updateMessage() {
    QLayoutItem *child;
    while ((child = m_messageLayout->takeAt(0)) != nullptr) {
        child->widget()->setParent(nullptr);
        delete child;
    }

    for (const auto &message : m_message) {
        auto label = new QLabel;
        label->setText(message);
        label->setStyleSheet("color: #808080");
        m_messageLayout->addWidget(label);
    }
}