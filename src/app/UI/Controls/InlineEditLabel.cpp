//
// Created by FlutyDeer on 2026/7/12.
//

#include "InlineEditLabel.h"
#include "InlineTextEditOverlay.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaEnum>
#include <QResizeEvent>
#include <QValidator>

#include <utility>

InlineEditLabel::InlineEditLabel(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_label = new QLabel;
    m_label->setText(QStringLiteral(" "));
    m_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_label->setWordWrap(false);
    m_label->setContentsMargins(8, 0, 0, 0);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);
    setLayout(layout);
}

InlineEditLabel::~InlineEditLabel() {
    if (m_overlay && m_overlay->parentWidget())
        m_overlay->parentWidget()->removeEventFilter(this);
    delete m_overlay;
}

QString InlineEditLabel::text() const {
    return m_label ? m_label->text() : m_text;
}

void InlineEditLabel::setText(const QString &text) {
    m_text = text;
    if (m_label)
        m_label->setText(text);
    updateGeometry();
}

QFont InlineEditLabel::displayFont() const {
    return m_displayFont;
}

void InlineEditLabel::setDisplayFont(const QFont &font) {
    m_displayFont = font;
    if (m_label)
        m_label->setFont(font);
    updateGeometry();
}

Qt::Alignment InlineEditLabel::alignment() const {
    return m_alignment;
}

void InlineEditLabel::setAlignment(const Qt::Alignment alignment) {
    m_alignment = alignment;
    if (m_label)
        m_label->setAlignment(alignment);
}

void InlineEditLabel::setTextMargins(const QMargins &margins) {
    if (m_label)
        m_label->setContentsMargins(margins);
    updateGeometry();
}

QValidator *InlineEditLabel::validator() const {
    return m_validator;
}

void InlineEditLabel::setValidator(QValidator *validator) {
    m_validator = validator;
}

void InlineEditLabel::setCommitValidator(std::function<bool(const QString &)> validator) {
    m_commitValidator = std::move(validator);
}

InlineEditLabel::EditRole InlineEditLabel::editRole() const {
    return m_editRole;
}

void InlineEditLabel::setEditRole(const EditRole role) {
    m_editRole = role;
    const auto metaEnum = QMetaEnum::fromType<EditRole>();
    setProperty("editRole", QString::fromLatin1(metaEnum.valueToKey(role)));
}

void InlineEditLabel::setOverlayParent(QWidget *parent) {
    if (m_overlayParent)
        m_overlayParent->removeEventFilter(this);
    m_overlayParent = parent;
}

void InlineEditLabel::finishEditing() {
    if (m_overlay && m_overlay->isEditing()) {
        m_overlay->submit();
    }
}

QSize InlineEditLabel::sizeHint() const {
    if (m_label)
        return m_label->sizeHint();
    return {40, 24};
}

QSize InlineEditLabel::minimumSizeHint() const {
    if (m_label)
        return m_label->minimumSizeHint();
    return {4, 24};
}

void InlineEditLabel::mouseDoubleClickEvent(QMouseEvent *event) {
    startEditing();
    event->accept();
}

void InlineEditLabel::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    finishEditing();
}

bool InlineEditLabel::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_overlayParent && m_overlay && m_overlay->isEditing()) {
        switch (event->type()) {
            case QEvent::Wheel:
            case QEvent::Move:
            case QEvent::Resize:
            case QEvent::Hide:
                finishEditing();
                break;
            default:
                break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void InlineEditLabel::startEditing() {
    if (m_overlay && m_overlay->isEditing())
        return;

    auto *overlayParent = m_overlayParent ? m_overlayParent.data() : parentWidget();
    if (!overlayParent)
        return;
    overlayParent->installEventFilter(this);

    if (!m_overlay) {
        m_overlay = new InlineTextEditOverlay(overlayParent);
        connect(m_overlay.data(), &InlineTextEditOverlay::textSubmitted, this,
                &InlineEditLabel::onTextSubmitted);
        connect(m_overlay.data(), &InlineTextEditOverlay::editCancelled, this,
                &InlineEditLabel::onEditCancelled);
    }

    const auto globalTopLeft = mapToGlobal(QPoint(0, 0));
    const auto localTopLeft = overlayParent->mapFromGlobal(globalTopLeft);
    const auto anchorRect = QRect(localTopLeft, size());

    const auto displayFont = m_displayFont.family().isEmpty() ? font() : m_displayFont;

    const auto metaEnum = QMetaEnum::fromType<EditRole>();
    const QVariantMap editorProperties = {
        {QStringLiteral("editRole"), QString::fromLatin1(metaEnum.valueToKey(m_editRole))},
        {QStringLiteral("groupPos"), property("groupPos")},
    };
    m_overlay->showAt(anchorRect, m_text, displayFont, editorProperties);

    if (m_overlay) {
        if (auto *lineEdit = m_overlay->findChild<QLineEdit *>()) {
            lineEdit->setAlignment(m_alignment);
            lineEdit->setValidator(m_validator);
        }
    }

    emit editingStarted();
}

void InlineEditLabel::onTextSubmitted(const QString &text) {
    if (m_overlay && m_overlay->parentWidget())
        m_overlay->parentWidget()->removeEventFilter(this);
    if (text == m_text)
        return;
    if (m_commitValidator && !m_commitValidator(text))
        return;

    m_text = text;
    if (m_label)
        m_label->setText(text);
    emit editCompleted(text);
}

void InlineEditLabel::onEditCancelled() {
    if (m_overlay && m_overlay->parentWidget())
        m_overlay->parentWidget()->removeEventFilter(this);
}
