#include "OptionSubPageStack.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QWheelEvent>

#include <lite/GUI/Animation/AnimationUtils.h>
#include <lite/GUI/Theme/ThemeManager.h>

using namespace OptionSubPage;

SubPageTransitionScrim::SubPageTransitionScrim(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setVisible(false);
}

qreal SubPageTransitionScrim::dim() const {
    return m_dim;
}

void SubPageTransitionScrim::setDim(qreal dim) {
    const auto bounded = qBound(0.0, dim, 1.0);
    if (qFuzzyCompare(m_dim, bounded))
        return;
    m_dim = bounded;
    update();
}

void SubPageTransitionScrim::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (m_dim <= 0.0)
        return;
    QPainter painter(this);
    auto color = QColor(0, 0, 0);
    color.setAlphaF(m_dim);
    painter.fillRect(rect(), color);
}

SubPageTransitionShield::SubPageTransitionShield(QWidget *parent) : QWidget(parent) {
    setVisible(false);
}

void SubPageTransitionShield::wheelEvent(QWheelEvent *event) {
    event->accept();
}

void SubPageTransitionShield::mousePressEvent(QMouseEvent *event) {
    event->accept();
}

void SubPageTransitionShield::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
}

void SubPageTransitionShield::mouseMoveEvent(QMouseEvent *event) {
    event->accept();
}

void SubPageTransitionShield::mouseDoubleClickEvent(QMouseEvent *event) {
    event->accept();
}

SubPageTransitionBackdrop::SubPageTransitionBackdrop(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setVisible(false);
}

void SubPageTransitionBackdrop::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    // The option pages are transparent by design and show the dialog backdrop,
    // which the theme paints from the surface.input token (see the QDialog rule
    // in controls.qss). Filling the same colour keeps a sliding page opaque
    // without changing how the page looks when it is idle.
    const auto color = ThemeManager::instance()->semanticColor(QStringLiteral("surface.input"));
    if (!color.isValid())
        return;
    QPainter painter(this);
    painter.fillRect(rect(), color);
}

OptionSubPageStack::OptionSubPageStack(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_scrim = new SubPageTransitionScrim(this);
    m_backdrop = new SubPageTransitionBackdrop(this);
    m_shield = new SubPageTransitionShield(this);
    initializeAnimation();
}

void OptionSubPageStack::addPage(QWidget *page) {
    if (!page || m_pages.contains(page))
        return;
    page->setParent(this);
    page->setVisible(false);
    page->setGeometry(rect());
    m_pages.append(page);
}

void OptionSubPageStack::setCurrentPage(QWidget *page, Direction direction, bool animated) {
    if (!page || !m_pages.contains(page) || page == m_current)
        return;

    auto *from = m_current;
    const int duration = animated ? getEffectiveAnimationTime(kTransitionMs) : 0;
    if (from && duration > 0 && isVisible()) {
        m_current = page;
        startTransition(from, page, direction, duration);
    } else {
        stopTransition();
        m_current = page;
        for (auto *candidate : std::as_const(m_pages))
            candidate->setVisible(candidate == m_current);
        if (m_current)
            m_current->setGeometry(rect());
    }
    // The visible page changed: the parent scroll area must re-read the hints.
    updateGeometry();
}

QWidget *OptionSubPageStack::currentPage() const {
    return m_current;
}

bool OptionSubPageStack::isTransitioning() const {
    return m_transitioning;
}

QSize OptionSubPageStack::sizeHint() const {
    return m_current ? m_current->sizeHint() : QWidget::sizeHint();
}

QSize OptionSubPageStack::minimumSizeHint() const {
    return m_current ? m_current->minimumSizeHint() : QWidget::minimumSizeHint();
}

bool OptionSubPageStack::hasHeightForWidth() const {
    // Always answer: pages without heightForWidth fall back to their minimum in
    // heightForWidth() below, which is what the surrounding layout would report
    // for them anyway.
    return true;
}

int OptionSubPageStack::heightForWidth(int width) const {
    if (!m_current)
        return -1;
    if (m_current->hasHeightForWidth())
        return m_current->heightForWidth(width);
    // A page that fills the viewport (pinned header plus its own inner scroll
    // area) has no heightForWidth of its own: its minimum is exactly the part
    // that must stay in view, and the scroll area clamps the page to at least
    // the viewport height.
    return m_current->minimumSizeHint().height();
}

void OptionSubPageStack::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    applyPageGeometry();
}

void OptionSubPageStack::afterSetAnimationEnabled(bool enabled) {
    Q_UNUSED(enabled)
    // Animation settings changed mid-flight: land on the target page.
    stopTransition();
}

void OptionSubPageStack::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    stopTransition();
}

void OptionSubPageStack::applyPageGeometry() const {
    for (auto *page : std::as_const(m_pages)) {
        if (m_transitioning && page == m_current) {
            // The incoming page is the container's next state: it follows the
            // size (the surrounding scroll area refits right after the switch).
            page->resize(width(), height());
            continue;
        }
        if (m_transitioning && page == m_outgoing) {
            // The leaving page keeps its own height. A content-sized page (one
            // that reports heightForWidth) must not be squeezed into the
            // container's new, possibly shorter height while it slides away:
            // that is what made the page being left behind show a collapsed
            // layout the moment the switch started. Pages without
            // heightForWidth fill the container (pinned header plus inner scroll
            // area), so those do follow it.
            page->resize(width(), page->hasHeightForWidth()
                                      ? qMax(height(), page->heightForWidth(width()))
                                      : height());
            continue;
        }
        page->setGeometry(rect());
    }
    if (m_shield)
        m_shield->setGeometry(rect());
    if (m_scrim) {
        // The scrim is parented to the page it covers while transitioning and
        // to the stack when idle: either way it covers its parent.
        if (const auto *host = m_scrim->parentWidget())
            m_scrim->setGeometry(host == this ? rect() : host->rect());
    }
    if (m_backdrop) {
        if (const auto *host = m_backdrop->parentWidget())
            m_backdrop->setGeometry(host == this ? rect() : host->rect());
    }
}

void OptionSubPageStack::startTransition(QWidget *from, QWidget *to, Direction direction,
                                         int duration) {
    stopTransition();
    m_transitioning = true;
    m_outgoing = from;

    const int shift = qRound(width() * kOutgoingShiftRatio);
    const bool forward = direction == Direction::Forward;
    // Which page stays on top: the incoming one on a push (it slides over the
    // leaving page), the leaving one on a back (the previous page is revealed
    // from underneath it).
    QWidget *top = forward ? to : from;
    QWidget *dimmed = forward ? from : to;

    to->setGeometry(QRect(forward ? width() : -shift, 0, width(), height()));
    to->show();
    from->show();
    from->move(0, 0);
    top->raise();

    // The scrim is parented to the page being covered so it rides that page's
    // position animation, keeping the dim aligned while it moves.
    m_scrim->setParent(dimmed);
    m_scrim->setGeometry(dimmed->rect());
    m_scrim->raise();
    m_scrim->setDim(forward ? 0.0 : kScrimPeakDim);
    m_scrim->show();

    // The page on top carries an opaque cover at the bottom of its children:
    // the option pages are transparent, so without it whatever is underneath
    // shows through while the top page slides over it (the incoming page on a
    // push, the leaving page on a back).
    m_backdrop->setParent(top);
    m_backdrop->setGeometry(top->rect());
    m_backdrop->lower();
    m_backdrop->show();

    m_shield->setGeometry(rect());
    m_shield->raise();
    m_shield->show();

    auto *group = new QParallelAnimationGroup(this);
    const auto animatePos = [group, duration](QWidget *target, const QPoint &endValue) {
        auto *animation = new QPropertyAnimation(target, "pos", group);
        AnimationUtils::configureSilently(*animation, [animation, target, endValue, duration] {
            animation->setDuration(duration);
            animation->setStartValue(target->pos());
            animation->setEndValue(endValue);
            animation->setEasingCurve(QEasingCurve::OutCubic);
        });
    };
    animatePos(to, QPoint(0, 0));
    animatePos(from, forward ? QPoint(-shift, 0) : QPoint(width(), 0));

    auto *scrimAnimation = new QPropertyAnimation(m_scrim, "dim", group);
    AnimationUtils::configureSilently(*scrimAnimation, [scrimAnimation, duration, forward] {
        scrimAnimation->setDuration(duration);
        scrimAnimation->setStartValue(forward ? 0.0 : kScrimPeakDim);
        scrimAnimation->setEndValue(forward ? kScrimPeakDim : 0.0);
        scrimAnimation->setEasingCurve(QEasingCurve::OutCubic);
    });

    connect(group, &QParallelAnimationGroup::finished, this, &OptionSubPageStack::stopTransition);
    m_group = group;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void OptionSubPageStack::stopTransition() {
    if (!m_transitioning)
        return;
    m_transitioning = false;

    if (m_group) {
        const auto group = m_group;
        m_group = nullptr;
        group->stop(); // DeleteWhenStopped cleans it up (deferred)
    }

    if (m_outgoing) {
        m_outgoing->move(0, 0);
        m_outgoing->setVisible(false);
        m_outgoing = nullptr;
    }
    if (m_scrim) {
        m_scrim->setDim(0.0);
        m_scrim->setParent(this);
        m_scrim->setGeometry(rect());
        m_scrim->setVisible(false);
    }
    if (m_backdrop) {
        m_backdrop->setParent(this);
        m_backdrop->setGeometry(rect());
        m_backdrop->setVisible(false);
    }
    if (m_shield)
        m_shield->setVisible(false);

    for (auto *page : std::as_const(m_pages))
        page->setVisible(page == m_current);
    if (m_current)
        m_current->setGeometry(rect());
}
