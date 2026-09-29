#include <lite/GUI/Controls/InlineTextEditOverlay.h>

#include <lite/GUI/Controls/LineEdit.h>
#include <lite/GUI/Controls/Menu.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyle>
#include <QTimer>

#include <cmath>

namespace {
    // Long press that opens the line edit's context menu with a finger. The
    // canvas gesture machine uses the same numbers, but its config lives in
    // the app layer and cannot be reached from this control.
    constexpr int kTouchLongPressMs = 450;
    constexpr double kTouchLongPressSlopPx = 12.0;

    class InlineLineEdit final : public LineEdit {
    public:
        using LineEdit::LineEdit;

    protected:
        void mousePressEvent(QMouseEvent *event) override {
            LineEdit::mousePressEvent(event);
            event->accept();
        }
    };
}

InlineTextEditOverlay::InlineTextEditOverlay(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("InlineTextEditOverlay");
    m_touchLongPressTimer = new QTimer(this);
    m_touchLongPressTimer->setSingleShot(true);
    m_touchLongPressTimer->setInterval(kTouchLongPressMs);
    connect(m_touchLongPressTimer, &QTimer::timeout, this, [this] {
        // The menu is owed to the release, not to the hold: a stream whose
        // release never shows up opens nothing at all instead of a menu the
        // finger never asked for.
        if (m_touchClaimed && !m_activeMenu && !m_submitted && isVisible())
            m_touchMenuPending = true;
    });
    hide();
}

void InlineTextEditOverlay::showAt(const QRect &anchorRect, const QString &text,
                                   const QFont &font, const QVariantMap &editorProperties) {
    m_submitted = false;
    m_activeMenu.clear();
    m_navigationEnabled = editorProperties.value(QStringLiteral("navigationEnabled")).toBool();

    if (!m_lineEdit) {
        m_lineEdit = new InlineLineEdit(this);
        m_lineEdit->setObjectName("inlineEditLineEdit");
        m_lineEdit->setFrame(false);
        m_lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_lineEdit->setMinimumSize(40, 20);
        m_lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);
        m_lineEdit->installEventFilter(this);
        connect(m_lineEdit, &QLineEdit::returnPressed, this, &InlineTextEditOverlay::submit);
        connect(m_lineEdit, &QLineEdit::customContextMenuRequested, this,
                [this](const QPoint &pos) { showContextMenu(m_lineEdit->mapToGlobal(pos)); });

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

bool InlineTextEditOverlay::relayTouchBegin(const QPointF &globalPos) {
    if (!m_lineEdit || !isEditing() || m_submitted)
        return false;
    if (!rect().contains(mapFromGlobal(globalPos).toPoint()))
        return false;
    // A stream whose end never arrived (a submit hid the line edit, the
    // platform dropped the release) must not leave it pressed: close the old
    // stream before opening the new one.
    if (m_touchClaimed)
        sendTouchMouse(QEvent::MouseButtonRelease, m_touchLastGlobal, Qt::LeftButton,
                       Qt::NoButton);
    m_touchClaimed = true;
    m_touchMenuPending = false;
    m_mouseTarget = m_lineEdit;
    m_touchPressGlobal = globalPos;
    m_touchLastGlobal = globalPos;
    if (!m_activeMenu)
        m_touchLongPressTimer->start();
    sendTouchMouse(QEvent::MouseButtonPress, globalPos, Qt::LeftButton, Qt::LeftButton);
    return true;
}

void InlineTextEditOverlay::relayTouchMove(const QPointF &globalPos) {
    if (!m_touchClaimed)
        return;
    m_touchLastGlobal = globalPos;
    const auto travel = globalPos - m_touchPressGlobal;
    if (std::hypot(travel.x(), travel.y()) > kTouchLongPressSlopPx) {
        m_touchLongPressTimer->stop();
        m_touchMenuPending = false;
    }
    sendTouchMouse(QEvent::MouseMove, globalPos, Qt::NoButton, Qt::LeftButton);
}

void InlineTextEditOverlay::relayTouchEnd(const QPointF &globalPos) {
    if (!m_touchClaimed)
        return;
    m_touchLongPressTimer->stop();
    const bool raiseMenu = m_touchMenuPending;
    m_touchMenuPending = false;
    sendTouchMouse(QEvent::MouseButtonRelease, globalPos, Qt::LeftButton, Qt::NoButton);
    if (raiseMenu)
        postTouchContextMenu();
    m_touchClaimed = false;
    m_mouseTarget.clear();
}

void InlineTextEditOverlay::relayTouchCancel() {
    if (!m_touchClaimed)
        return;
    m_touchLongPressTimer->stop();
    m_touchMenuPending = false;
    sendTouchMouse(QEvent::MouseButtonRelease, m_touchLastGlobal, Qt::LeftButton, Qt::NoButton);
    m_touchClaimed = false;
    m_mouseTarget.clear();
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
                if (m_navigationEnabled &&
                    (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab)) {
                    const bool backwards = keyEvent->key() == Qt::Key_Backtab ||
                                           keyEvent->modifiers().testFlag(Qt::ShiftModifier);
                    navigate(backwards);
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
            default:
                break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void InlineTextEditOverlay::sendTouchMouse(const QEvent::Type type, const QPointF &globalPos,
                                           const Qt::MouseButton button,
                                           const Qt::MouseButtons buttons) {
    auto *target = m_mouseTarget.data();
    if (!target)
        return;
    QMouseEvent mouseEvent(type, target->mapFromGlobal(globalPos), globalPos, globalPos, button,
                           buttons, QApplication::keyboardModifiers());
    QCoreApplication::sendEvent(target, &mouseEvent);
}

void InlineTextEditOverlay::postTouchContextMenu() {
    if (m_activeMenu || m_submitted || !isVisible() || !m_mouseTarget)
        return;
    // Posted, never sent: raising the menu while the canvas controller is
    // still forwarding the stream would run its event loop under its feet.
    const auto global = m_touchLastGlobal.toPoint();
    QCoreApplication::postEvent(
        m_mouseTarget.data(),
        new QContextMenuEvent(QContextMenuEvent::Mouse, m_mouseTarget->mapFromGlobal(global),
                              global, Qt::NoModifier));
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

void InlineTextEditOverlay::navigate(const bool backwards) {
    if (m_submitted)
        return;
    m_submitted = true;
    qApp->removeEventFilter(this);
    const auto text = m_lineEdit ? m_lineEdit->text() : QString();
    hide();
    emit navigationRequested(text, backwards);
}

void InlineTextEditOverlay::showContextMenu(const QPoint &globalPos) {
    if (!m_lineEdit || !isEditing() || m_submitted || m_activeMenu)
        return;

    if (const auto menu = m_lineEdit->createContextMenu(this)) {
        menu->setAttribute(Qt::WA_DeleteOnClose);
        m_activeMenu = menu;
        connect(menu, &QObject::destroyed, this, [this] {
            m_activeMenu.clear();
            if (m_lineEdit && isEditing() && !m_submitted)
                m_lineEdit->setFocus();
        });
        menu->popup(globalPos);
    }
}
