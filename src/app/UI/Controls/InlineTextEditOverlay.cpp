//
// Created by FlutyDeer on 2026/7/12.
//

#include "InlineTextEditOverlay.h"

#include "Menu.h"

#include <QApplication>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyle>

InlineTextEditOverlay::InlineTextEditOverlay(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("InlineTextEditOverlay");
    hide();
}

void InlineTextEditOverlay::showAt(const QRect &anchorRect, const QString &text,
                                   const QFont &font, const QVariantMap &editorProperties) {
    m_submitted = false;
    m_activeMenu.clear();

    if (!m_lineEdit) {
        m_lineEdit = new QLineEdit(this);
        m_lineEdit->setObjectName("inlineEditLineEdit");
        m_lineEdit->setFrame(false);
        m_lineEdit->installEventFilter(this);
        connect(m_lineEdit, &QLineEdit::returnPressed, this, &InlineTextEditOverlay::submit);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_lineEdit);
        setLayout(layout);
    }

    m_lineEdit->setFont(font);
    for (auto it = editorProperties.cbegin(); it != editorProperties.cend(); ++it)
        m_lineEdit->setProperty(it.key().toUtf8().constData(), it.value());
    m_lineEdit->style()->unpolish(m_lineEdit);
    m_lineEdit->style()->polish(m_lineEdit);
    m_lineEdit->setText(text);
    m_lineEdit->selectAll();

    setGeometry(anchorRect);
    show();
    raise();
    m_lineEdit->setFocus();
    qApp->installEventFilter(this);
}

void InlineTextEditOverlay::dismiss(const bool cancel) {
    qApp->removeEventFilter(this);
    hide();
    if (cancel && !m_submitted) {
        m_submitted = true;
        emit editCancelled();
    }
}

bool InlineTextEditOverlay::isEditing() const {
    return isVisible();
}

bool InlineTextEditOverlay::eventFilter(QObject *obj, QEvent *event) {
    if (isVisible() && !m_submitted && !m_activeMenu) {
        if (event->type() == QEvent::MouseButtonPress) {
            if (auto *widget = qobject_cast<QWidget *>(obj)) {
                if (widget != this && !isAncestorOf(widget)) {
                    submit();
                    return false;
                }
            }
        } else if (event->type() == QEvent::Wheel ||
                   event->type() == QEvent::ApplicationDeactivate) {
            submit();
            return false;
        } else if (auto *widget = qobject_cast<QWidget *>(obj)) {
            const auto type = event->type();
            const bool hostGeometryChanged =
                type == QEvent::Move || type == QEvent::Resize || type == QEvent::Hide ||
                type == QEvent::ParentAboutToChange || type == QEvent::ParentChange ||
                type == QEvent::WindowDeactivate || type == QEvent::WindowStateChange;
            if (hostGeometryChanged && widget != this &&
                (widget == parentWidget() || widget->isAncestorOf(this))) {
                submit();
                return false;
            }
        }
    }
    if (obj == m_lineEdit) {
        switch (event->type()) {
            case QEvent::KeyPress: {
                auto *keyEvent = static_cast<QKeyEvent *>(event);
                if (keyEvent->key() == Qt::Key_Escape) {
                    cancel();
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                    // Let QLineEdit handle the return press first, then submit via
                    // returnPressed signal
                    break;
                }
                break;
            }
            case QEvent::FocusOut: {
                if (!m_activeMenu) {
                    submit();
                }
                break;
            }
            case QEvent::ContextMenu: {
                // Use QLineEdit's standard context menu, wrapped in project Menu
                if (const auto qMenu = m_lineEdit->createStandardContextMenu()) {
                    auto *menu = new Menu(this);
                    for (const auto action : qMenu->actions()) {
                        action->setParent(menu);
                        menu->addAction(action);
                    }
                    delete qMenu;

                    menu->setAttribute(Qt::WA_DeleteOnClose);
                    m_activeMenu = menu;

                    connect(menu, &QObject::destroyed, this, [this] {
                        m_activeMenu.clear();
                        if (m_lineEdit && isEditing() && !m_submitted) {
                            m_lineEdit->setFocus();
                        }
                    });

                    auto *ctxEvent = static_cast<QContextMenuEvent *>(event);
                    menu->popup(ctxEvent->globalPos());
                }

                event->accept();
                return true;
            }
            default:
                break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void InlineTextEditOverlay::submit() {
    if (m_submitted)
        return;
    m_submitted = true;
    qApp->removeEventFilter(this);
    const auto text = m_lineEdit ? m_lineEdit->text() : QString();
    hide();
    emit textSubmitted(text);
}

void InlineTextEditOverlay::cancel() {
    if (m_submitted)
        return;
    m_submitted = true;
    qApp->removeEventFilter(this);
    hide();
    emit editCancelled();
}
