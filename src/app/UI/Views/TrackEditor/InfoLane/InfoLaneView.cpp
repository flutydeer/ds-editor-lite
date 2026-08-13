#include "InfoLaneView.h"

#include "Controller/PlaybackController.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

namespace {
    constexpr int markerWidth = 2;
    constexpr int textLeftPadding = 6;   // from the marker's left edge
    constexpr int textRightPadding = 6;
    constexpr int chipMinGap = 2;        // keep neighbor chips visually apart

    QColor blendColor(const QColor &from, const QColor &to, double ratio) {
        if (ratio < 0)
            ratio = 0;
        else if (ratio > 1)
            ratio = 1;
        return QColor(static_cast<int>(from.red() + (to.red() - from.red()) * ratio),
                      static_cast<int>(from.green() + (to.green() - from.green()) * ratio),
                      static_cast<int>(from.blue() + (to.blue() - from.blue()) * ratio));
    }
}

InfoLaneView::InfoLaneView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setMouseTracking(true);

    const auto applyTimeline = [this] {
        setTimeline(appModel->timeline());
        update();
    };
    applyTimeline();
    connect(appModel, &AppModel::modelChanged, this, applyTimeline);
    connect(appModel, &AppModel::timelineChanged, this, applyTimeline);

    m_position = playbackController->position();
    m_lastPosition = playbackController->lastPosition();
    connect(playbackController, &PlaybackController::visualPositionChanged, this,
            &InfoLaneView::setPosition);
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            &InfoLaneView::setLastPosition);
}

void InfoLaneView::setTimeRange(const double startTick, const double endTick) {
    m_startTick = startTick;
    m_endTick = endTick;
    update();
}

void InfoLaneView::setChips(QList<Chip> chips) {
    m_chips = std::move(chips);
    m_hoveredChip = -1;
    update();
}

const QList<InfoLaneView::Chip> &InfoLaneView::chips() const {
    return m_chips;
}

int InfoLaneView::chipIndexAt(const QPoint &pos) const {
    for (int i = 0; i < m_chips.size(); i++) {
        if (chipRect(i).contains(pos))
            return i;
    }
    return -1;
}

double InfoLaneView::tickToX(const double tick) const {
    if (m_endTick <= m_startTick)
        return 0;
    return rect().width() * (tick - m_startTick) / (m_endTick - m_startTick);
}

double InfoLaneView::xToTick(const double x) const {
    if (rect().width() <= 0)
        return m_startTick;
    const auto tick = x / rect().width() * (m_endTick - m_startTick) + m_startTick;
    return tick < 0 ? 0 : tick;
}

void InfoLaneView::chipDoubleClicked(const Chip &chip) {
    Q_UNUSED(chip);
}

void InfoLaneView::blankDoubleClicked(const QPoint &pos) {
    Q_UNUSED(pos);
}

void InfoLaneView::chipContextMenuRequested(const Chip &chip, const QPoint &globalPos) {
    Q_UNUSED(chip);
    Q_UNUSED(globalPos);
}

void InfoLaneView::blankContextMenuRequested(const QPoint &globalPos) {
    Q_UNUSED(globalPos);
}

QRectF InfoLaneView::chipRect(const int index) const {
    const auto &chip = m_chips.at(index);
    const auto x = tickToX(chip.tick);
    auto right = x + textLeftPadding + fontMetrics().horizontalAdvance(chip.text) +
                 textRightPadding;
    if (index + 1 < m_chips.size())
        right = qMin(right, tickToX(m_chips.at(index + 1).tick) - chipMinGap);
    return {x, 0.0, qMax(right - x, static_cast<double>(markerWidth)),
            static_cast<double>(height())};
}

void InfoLaneView::setHoveredChip(const int index) {
    if (m_hoveredChip == index)
        return;
    m_hoveredChip = index;
    update();
}

void InfoLaneView::setPosition(const double tick) {
    m_position = tick;
    update();
}

void InfoLaneView::setLastPosition(const double tick) {
    m_lastPosition = tick;
    update();
}

void InfoLaneView::drawBar(QPainter *painter, const int tick, const int bar) {
    Q_UNUSED(bar);
    const auto x = tickToX(tick);
    painter->setPen(m_barLineColor);
    painter->drawLine(QLineF(x, 0, x, height()));
}

void InfoLaneView::drawBeat(QPainter *painter, const int tick, const int bar, const int beat) {
    Q_UNUSED(bar);
    Q_UNUSED(beat);
    const auto x = tickToX(tick);
    painter->setPen(m_beatLineColor);
    painter->drawLine(QLineF(x, 0, x, height()));
}

void InfoLaneView::drawSubdivision(QPainter *painter, const int tick, const int level,
                                   const int levelCount) {
    const auto x = tickToX(tick);
    const double ratio = levelCount > 1 ? static_cast<double>(level) / (levelCount - 1) : 0.0;
    painter->setPen(blendColor(m_beatLineColor, m_commonLineColor, ratio));
    painter->drawLine(QLineF(x, 0, x, height()));
}

void InfoLaneView::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    drawTimeline(&painter, m_startTick, m_endTick, rect().width());
    for (int i = 0; i < m_chips.size(); i++) {
        const auto chipArea = chipRect(i);
        if (chipArea.right() < 0 || chipArea.left() > width())
            continue;

        if (i == m_hoveredChip)
            painter.fillRect(chipArea, m_hoverFillColor);
        painter.fillRect(QRectF(chipArea.left(), 0, markerWidth, height()), m_markerColor);

        const QRectF textArea(chipArea.left() + textLeftPadding, 0,
                              chipArea.width() - textLeftPadding, height());
        if (textArea.width() < 4)
            continue;
        const auto text = fontMetrics().elidedText(m_chips.at(i).text, Qt::ElideRight,
                                                   qFloor(textArea.width()));
        painter.setPen(m_textColor);
        painter.drawText(textArea, Qt::AlignLeft | Qt::AlignVCenter, text);
    }

    // Playhead lines on top, continuing the canvas indicators through the lane
    const auto drawPlayheadLine = [&](const double tick, const QColor &color,
                                      const Qt::PenStyle style) {
        const auto x = tickToX(tick);
        if (x < 0 || x > width())
            return;
        QPen pen(color);
        pen.setStyle(style);
        painter.setPen(pen);
        painter.drawLine(QLineF(x, 0, x, height()));
    };
    drawPlayheadLine(m_lastPosition, m_lastPlayheadColor, Qt::DashLine);
    drawPlayheadLine(m_position, m_playheadColor, Qt::SolidLine);
}

void InfoLaneView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() == Qt::ControlModifier)
        emit wheelHorScale(event);
    else if (event->modifiers() == Qt::ShiftModifier)
        emit wheelHorScroll(event);
    else
        emit wheelVerScroll(event);
}

void InfoLaneView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    const auto index = chipIndexAt(event->pos());
    if (index >= 0)
        chipDoubleClicked(m_chips.at(index));
    else
        blankDoubleClicked(event->pos());
    event->accept();
}

void InfoLaneView::contextMenuEvent(QContextMenuEvent *event) {
    const auto index = chipIndexAt(event->pos());
    if (index >= 0)
        chipContextMenuRequested(m_chips.at(index), event->globalPos());
    else
        blankContextMenuRequested(event->globalPos());
    event->accept();
}

void InfoLaneView::mouseMoveEvent(QMouseEvent *event) {
    setHoveredChip(chipIndexAt(event->pos()));
    QWidget::mouseMoveEvent(event);
}

void InfoLaneView::leaveEvent(QEvent *event) {
    setHoveredChip(-1);
    QWidget::leaveEvent(event);
}

QColor InfoLaneView::textColor() const {
    return m_textColor;
}

void InfoLaneView::setTextColor(const QColor &color) {
    if (m_textColor == color)
        return;
    m_textColor = color;
    update();
}

QColor InfoLaneView::markerColor() const {
    return m_markerColor;
}

void InfoLaneView::setMarkerColor(const QColor &color) {
    if (m_markerColor == color)
        return;
    m_markerColor = color;
    update();
}

QColor InfoLaneView::hoverFillColor() const {
    return m_hoverFillColor;
}

void InfoLaneView::setHoverFillColor(const QColor &color) {
    if (m_hoverFillColor == color)
        return;
    m_hoverFillColor = color;
    update();
}

QColor InfoLaneView::barLineColor() const {
    return m_barLineColor;
}

void InfoLaneView::setBarLineColor(const QColor &color) {
    if (m_barLineColor == color)
        return;
    m_barLineColor = color;
    update();
}

QColor InfoLaneView::beatLineColor() const {
    return m_beatLineColor;
}

void InfoLaneView::setBeatLineColor(const QColor &color) {
    if (m_beatLineColor == color)
        return;
    m_beatLineColor = color;
    update();
}

QColor InfoLaneView::commonLineColor() const {
    return m_commonLineColor;
}

void InfoLaneView::setCommonLineColor(const QColor &color) {
    if (m_commonLineColor == color)
        return;
    m_commonLineColor = color;
    update();
}

QColor InfoLaneView::playheadColor() const {
    return m_playheadColor;
}

void InfoLaneView::setPlayheadColor(const QColor &color) {
    if (m_playheadColor == color)
        return;
    m_playheadColor = color;
    update();
}

QColor InfoLaneView::lastPlayheadColor() const {
    return m_lastPlayheadColor;
}

void InfoLaneView::setLastPlayheadColor(const QColor &color) {
    if (m_lastPlayheadColor == color)
        return;
    m_lastPlayheadColor = color;
    update();
}
