#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QScreen>
#include <QVBoxLayout>
#include <QLabel>

#include <algorithm>

#include <lite/GUI/Controls/ToolTip.h>

namespace {
    constexpr int anchorGap = 4;

    // Clearance kept above the pointer by showAbovePointer(). An adult index
    // finger has an 8-10 mm contact patch and the pointer reports its centre,
    // so 10 mm clears the whole fingertip with a little to spare. Expressed in
    // millimetres so it stays a real-world distance on every DPI and on scaled
    // touch screens, where a fixed pixel gap is either useless or absurd.
    constexpr double pointerClearanceMm = 10.0;
    constexpr double mmPerInch = 25.4;

    // Screens that report no usable physical size (offscreen platforms report
    // negative values) would otherwise turn the clearance into garbage.
    constexpr double fallbackDotsPerInch = 96.0;
    constexpr int minimumPointerClearancePx = 24;
}

ToolTip::ToolTip(const QString &title, QWidget *parent) : QFrame(parent) {
    m_lbTitle = new QLabel(title);
    m_lbTitle->setObjectName("toolTipTitle");
    m_lbTitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    // The rich-text label may end up wider than its text when the card is
    // stretched by long message lines; keep the title on the shared left edge
    m_lbTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_lbShortcutKey = new QLabel();
    m_lbShortcutKey->setObjectName("toolTipShortcutKey");
    m_lbShortcutKey->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_lbShortcutKey->setVisible(false);

    const auto titleShortcutLayout = new QHBoxLayout;
    titleShortcutLayout->addWidget(m_lbTitle);
    titleShortcutLayout->addWidget(m_lbShortcutKey);
    // qGeomCalc's last resort splits the row's leftover space between the
    // chain start and end, which centers a lone fixed-size title inside a card
    // stretched by long message lines; the stretch absorbs it and keeps the
    // title on the shared left edge
    titleShortcutLayout->addStretch(1);
    titleShortcutLayout->setContentsMargins({});

    m_messageLayout = new QVBoxLayout;
    m_messageLayout->setContentsMargins({});
    m_messageLayout->setSpacing(0);

    m_cardLayout = new QVBoxLayout;
    m_cardLayout->addLayout(titleShortcutLayout);
    m_cardLayout->addLayout(m_messageLayout);
    m_cardLayout->setContentsMargins({});

    const auto container = new QFrame;
    container->setObjectName("toolTipContainer");
    container->setLayout(m_cardLayout);
    container->setContentsMargins(8, 4, 8, 4);
    container->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(24);
    m_shadowEffect->setColor(QColor(0, 0, 0, 32));
    m_shadowEffect->setOffset(0, 4);
    container->setGraphicsEffect(m_shadowEffect);

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(container);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    setLayout(mainLayout);

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowOpacity(0);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    m_opacityAnimation = new QPropertyAnimation(this, "windowOpacity");
    m_opacityAnimation->setDuration(150);
    connect(m_opacityAnimation, &QPropertyAnimation::finished, this, [this] {
        if (windowOpacity() == 0) {
            hide();
            emit hideAnimationFinished();
        }
    });
    initializeAnimation();
}

ToolTip::~ToolTip() {
    delete m_opacityAnimation;
}

QColor ToolTip::shadowColor() const {
    return m_shadowEffect->color();
}

void ToolTip::setShadowColor(const QColor &color) {
    m_shadowEffect->setColor(color);
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

void ToolTip::setAnimationEnabled(bool on) {
    if (m_animationEnabled == on)
        return;
    m_animationEnabled = on;
    updateAnimationSettings();
}

bool ToolTip::animationEnabled() const {
    return m_animationEnabled;
}

const QScreen *ToolTip::resolveScreen(const QPoint &screenPos, const QScreen *screen) {
    if (screen)
        return screen;
    if (const auto *atPos = QApplication::screenAt(screenPos))
        return atPos;
    // A point in the seams of a multi-screen desktop resolves to nothing.
    return QApplication::primaryScreen();
}

int ToolTip::pointerClearance(const QScreen *screen) {
    // physicalDotsPerInch() is already reported in device-independent dots
    // (QScreen::size() is the logical size) and must not be divided by
    // devicePixelRatio() again. ComboPopupTouchFilter::pixelPerMeter() and
    // QScrollerPrivate::setDpiFromWidget() use the same convention.
    auto dpi = fallbackDotsPerInch;
    if (const auto *s = screen ? screen : QApplication::primaryScreen()) {
        const auto physical = s->physicalDotsPerInch();
        if (qIsFinite(physical) && physical > 0.0)
            dpi = physical;
    }
    return std::max(minimumPointerClearancePx, qRound(dpi / mmPerInch * pointerClearanceMm));
}

QPoint ToolTip::positionAbove(const QPoint &anchorPos, const QScreen *screen,
                              const int gapPx) const {
    const auto margins = layout()->contentsMargins();
    const auto contentWidth = width() - margins.left() - margins.right();
    const auto x = anchorPos.x() - contentWidth / 2 - margins.left();
    const auto aboveY = anchorPos.y() - gapPx - height() + margins.bottom();

    auto y = aboveY;
    if (screen) {
        const auto available = screen->availableGeometry();
        const auto belowY = anchorPos.y() + gapPx - margins.top();
        // Clamping to the top of the screen would push the card back under the
        // pointer, which is the very thing the gap exists to prevent; flip to
        // the other side instead, and only fall back to clamping when neither
        // side has room.
        if (aboveY < available.top() && belowY + height() <= available.bottom())
            y = belowY;
    }
    return {x, y};
}

QPoint ToolTip::clampToScreen(const QPoint &screenPos, const QScreen *screen) const {
    if (!screen)
        screen = resolveScreen(screenPos, nullptr);
    if (!screen)
        return screenPos;

    const auto screenRect = screen->availableGeometry();
    const auto toolTipRect = rect();
    const auto left = screenRect.left();
    const auto top = screenRect.top();
    const auto width = screenRect.width() - toolTipRect.width();
    const auto height = screenRect.height() - toolTipRect.height();
    const auto availableRect = QRect(left, top, width, height);

    auto x = screenPos.x();
    auto y = screenPos.y();

    if (x < availableRect.left())
        x = availableRect.left();
    else if (x > availableRect.right())
        x = availableRect.right();

    if (y < availableRect.top())
        y = availableRect.top();
    else if (y > availableRect.bottom())
        y = availableRect.bottom();

    return QPoint(x, y);
}

void ToolTip::showAt(const QPoint &screenPos) {
    showAt(screenPos, QApplication::screenAt(screenPos));
}

void ToolTip::showAt(const QPoint &screenPos, const QScreen *screen) {
    move(clampToScreen(screenPos, screen));

    if (m_animationEnabled && m_opacityAnimation->duration() > 0) {
        m_opacityAnimation->stop();
        m_opacityAnimation->setStartValue(windowOpacity());
        m_opacityAnimation->setEndValue(1);
        m_opacityAnimation->start();
    } else {
        setWindowOpacity(1);
    }

    show();
}

void ToolTip::resizeToContentHint() {
    // A content change only dirties the direct parent layout of the changed
    // label; the enclosing sub-layouts refresh their cached hints lazily
    // through a deferred LayoutRequest event, so a synchronously read
    // sizeHint() would size the card from the previous content and a reused
    // card would never shrink. activate() refreshes a layout and its
    // sub-layouts but stops at widget items, so the refresh is pushed through
    // the card's own layout first; the updateGeometry() it issues on the card
    // then also dirties the top-level layout for the re-read below
    if (auto *topLayout = layout()) {
        for (int i = 0; i < topLayout->count(); ++i) {
            auto *widget = topLayout->itemAt(i)->widget();
            if (widget && widget->layout())
                widget->layout()->activate();
        }
        topLayout->activate();
    }
    const auto hint = sizeHint();
    if (hint.isEmpty())
        return;
    setFixedSize(hint);
}

void ToolTip::showAbove(const QRect &screenRect) {
    resizeToContentHint();
    const auto *screen = resolveScreen(screenRect.center(), nullptr);
    showAt(positionAbove({screenRect.center().x(), screenRect.top()}, screen, anchorGap), screen);
}

void ToolTip::showAbovePointer(const QPoint &screenPos, const QScreen *screen) {
    resizeToContentHint();
    const auto *resolved = resolveScreen(screenPos, screen);
    showAt(positionAbove(screenPos, resolved, pointerClearance(resolved)), resolved);
}

void ToolTip::moveAbovePointer(const QPoint &screenPos, const QScreen *screen) {
    resizeToContentHint();
    const auto *resolved = resolveScreen(screenPos, screen);
    move(clampToScreen(positionAbove(screenPos, resolved, pointerClearance(resolved)), resolved));
}

void ToolTip::moveTo(const QPoint &screenPos) {
    move(clampToScreen(screenPos));
}

void ToolTip::hideWithAnimation() {
    if (m_animationEnabled && m_opacityAnimation->duration() > 0) {
        m_opacityAnimation->stop();
        m_opacityAnimation->setStartValue(windowOpacity());
        m_opacityAnimation->setEndValue(0);
        m_opacityAnimation->start();
    } else {
        setWindowOpacity(0);
        hide();
        emit hideAnimationFinished();
    }
}

void ToolTip::afterSetAnimationEnabled(bool enabled) {
    Q_UNUSED(enabled)
    updateAnimationSettings();
}

void ToolTip::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    updateAnimationSettings();
}

void ToolTip::updateAnimationSettings() {
    const auto running = m_opacityAnimation->state() == QAbstractAnimation::Running;
    const auto endValue = m_opacityAnimation->endValue();
    const auto duration = m_animationEnabled ? getEffectiveAnimationTime(150) : 0;
    m_opacityAnimation->stop();
    m_opacityAnimation->setDuration(duration);
    if (!running)
        return;
    if (duration == 0) {
        m_opacityAnimation->setEndValue(endValue);
        completeOpacityAnimation();
        return;
    }
    m_opacityAnimation->setStartValue(windowOpacity());
    m_opacityAnimation->setEndValue(endValue);
    m_opacityAnimation->start();
}

void ToolTip::completeOpacityAnimation() {
    const auto targetOpacity = m_opacityAnimation->endValue().toDouble();
    m_opacityAnimation->stop();
    setWindowOpacity(targetOpacity);
    if (qFuzzyIsNull(targetOpacity)) {
        hide();
        emit hideAnimationFinished();
    }
}

void ToolTip::updateMessage() {
    QLayoutItem *child;
    while ((child = m_messageLayout->takeAt(0)) != nullptr) {
        if (const auto widget = child->widget())
            widget->deleteLater();
        delete child;
    }

    for (const auto &message : m_message) {
        const auto label = new QLabel;
        label->setObjectName("toolTipMessage");
        label->setText(message);
        m_messageLayout->addWidget(label);
    }
}
