#include "OverlayScrollBar.h"

#include <QAbstractScrollArea>
#include <QEvent>
#include <QPainter>
#include <QScrollBar>
#include <QVariantAnimation>

static constexpr int kBarThickness = 14;
static constexpr int kHandleMargin = 4;
static constexpr int kHandleMinLength = 20;

OverlayScrollBar::OverlayScrollBar(Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);
    if (orientation == Qt::Horizontal)
        setFixedHeight(kBarThickness);
    else
        setFixedWidth(kBarThickness);

    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_opacity = value.toDouble();
        update();
    });
}

void OverlayScrollBar::attachTo(QAbstractScrollArea *scrollArea) {
    m_scrollArea = scrollArea;

    const bool horizontal = orientation() == Qt::Horizontal;
    if (horizontal)
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    else
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *source =
        horizontal ? scrollArea->horizontalScrollBar() : scrollArea->verticalScrollBar();

    connect(source, &QScrollBar::rangeChanged, this, [this](int min, int max) {
        setRange(min, max);
        setVisible(max > 0);
        updatePosition();
    });
    connect(source, &QScrollBar::valueChanged, this, &QScrollBar::setValue);
    connect(this, &QScrollBar::valueChanged, source, &QScrollBar::setValue);

    setRange(source->minimum(), source->maximum());
    setPageStep(source->pageStep());
    setSingleStep(source->singleStep());
    setVisible(source->maximum() > 0);

    connect(source, &QScrollBar::rangeChanged, this, [this, source] {
        setPageStep(source->pageStep());
        setSingleStep(source->singleStep());
    });

    scrollArea->viewport()->installEventFilter(this);
}

void OverlayScrollBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    if (maximum() <= 0)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int totalRange = maximum() - minimum() + pageStep();
    if (totalRange <= 0)
        return;

    const bool horizontal = orientation() == Qt::Horizontal;
    int availableLength = (horizontal ? width() : height()) - 2 * kHandleMargin;
    int handleLength = qMax(kHandleMinLength, availableLength * pageStep() / totalRange);
    int handlePos = kHandleMargin;
    if (maximum() > minimum())
        handlePos +=
            (availableLength - handleLength) * (value() - minimum()) / (maximum() - minimum());

    qreal baseOpacity = 0.25 + 0.10 * m_opacity;
    auto color = QColor(255, 255, 255, qRound(255 * baseOpacity));
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    qreal radius = m_hovered ? 3.0 : 2.0;
    qreal margin = m_hovered ? 3.0 : 4.0;

    if (horizontal)
        p.drawRoundedRect(QRectF(handlePos, margin, handleLength, height() - 2 * margin), radius,
                          radius);
    else
        p.drawRoundedRect(QRectF(margin, handlePos, width() - 2 * margin, handleLength), radius,
                          radius);
}

void OverlayScrollBar::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event)
    m_hovered = true;
    setHighlightVisible(true);
}

void OverlayScrollBar::leaveEvent(QEvent *event) {
    Q_UNUSED(event)
    m_hovered = false;
    setHighlightVisible(false);
}

bool OverlayScrollBar::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Resize)
        updatePosition();
    return QScrollBar::eventFilter(watched, event);
}

void OverlayScrollBar::setHighlightVisible(bool visible) {
    m_animation->stop();
    m_animation->setStartValue(m_opacity);
    if (visible) {
        m_animation->setEndValue(1.0);
        m_animation->setDuration(100);
    } else {
        m_animation->setEndValue(0.0);
        m_animation->setDuration(300);
    }
    m_animation->start();
}

void OverlayScrollBar::updatePosition() {
    if (!m_scrollArea)
        return;

    auto viewport = m_scrollArea->viewport();
    auto mapped = viewport->mapTo(parentWidget(), QPoint(0, 0));
    if (orientation() == Qt::Horizontal) {
        setGeometry(mapped.x(), mapped.y() + viewport->height() - kBarThickness, viewport->width(),
                    kBarThickness);
    } else {
        setGeometry(mapped.x() + viewport->width() - kBarThickness, mapped.y(), kBarThickness,
                    viewport->height());
    }
    raise();
}

OverlayScrollBar *OverlayScrollBar::install(QAbstractScrollArea *scrollArea,
                                             Qt::Orientation orientation) {
    auto *bar = new OverlayScrollBar(orientation, scrollArea);
    bar->attachTo(scrollArea);
    bar->updatePosition();
    return bar;
}
