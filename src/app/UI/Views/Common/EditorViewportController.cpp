#include "EditorViewportController.h"

#include "Global/AppGlobal.h"

#include <algorithm>
#include <cmath>

EditorViewportController::EditorViewportController(QObject *parent) : QObject(parent) {
}

void EditorViewportController::setPixelsPerQuarterNote(const double value) {
    if (!std::isfinite(value) || value <= 0.0 || qFuzzyCompare(value, m_pixelsPerQuarterNote))
        return;
    const auto previous = state();
    m_pixelsPerQuarterNote = value;
    restoreState(previous);
}

void EditorViewportController::setContentTickRange(const double startTick, const double endTick) {
    if (!std::isfinite(startTick) || !std::isfinite(endTick) || endTick < startTick)
        return;
    const auto previous = state();
    m_startTick = startTick;
    m_endTick = endTick;
    restoreState(previous);
}

void EditorViewportController::setVerticalContent(const double unitCount, const double unitHeight) {
    if (!std::isfinite(unitCount) || !std::isfinite(unitHeight) || unitCount < 0.0 ||
        unitHeight <= 0.0) {
        return;
    }
    const auto previous = state();
    m_unitCount = unitCount;
    m_unitHeight = unitHeight;
    restoreState(previous);
}

void EditorViewportController::setViewportSize(const QSizeF &size) {
    if (size == m_viewportSize)
        return;
    const auto previous = state();
    m_viewportSize = QSizeF(std::max(0.0, size.width()), std::max(0.0, size.height()));
    restoreState(previous);
}

void EditorViewportController::setScaleBounds(const double minX, const double maxX,
                                              const double minY, const double maxY) {
    if (minX <= 0.0 || minY <= 0.0 || maxX < minX || maxY < minY)
        return;
    m_minScaleX = minX;
    m_maxScaleX = maxX;
    m_minScaleY = minY;
    m_maxScaleY = maxY;
    normalize(true);
    notify(true);
}

void EditorViewportController::setEnsureContentFillsViewport(const bool horizontal,
                                                             const bool vertical) {
    m_fillX = horizontal;
    m_fillY = vertical;
    normalize(true);
    notify(true);
}

// RHI path: viewport scene coordinates are real pixels, so the margin is a
// plain pixel offset (independent of zoom), mirroring TimeGraphicsScene.
void EditorViewportController::setLeftMarginPx(const double px) {
    if (!std::isfinite(px) || px < 0.0 || qFuzzyCompare(px, m_leftMarginPx))
        return;
    const auto previous = state();
    m_leftMarginPx = px;
    restoreState(previous);
}

EditorViewportController::State EditorViewportController::state() const {
    return {
        .centerTick = (startTick() + endTick()) * 0.5,
        .centerUnit = (topUnit() + bottomUnit()) * 0.5,
        .horizontalScale = m_scaleX,
        .verticalScale = m_scaleY,
    };
}

bool EditorViewportController::restoreState(const State &state) {
    if (!std::isfinite(state.centerTick) || !std::isfinite(state.centerUnit) ||
        !std::isfinite(state.horizontalScale) || !std::isfinite(state.verticalScale) ||
        state.horizontalScale <= 0.0 || state.verticalScale <= 0.0) {
        return false;
    }
    m_scaleX = state.horizontalScale;
    m_scaleY = state.verticalScale;
    normalize(true);
    m_offsetX = tickToSceneX(state.centerTick) - m_viewportSize.width() * 0.5;
    m_offsetY = unitToSceneY(state.centerUnit) - m_viewportSize.height() * 0.5;
    normalize(false);
    notify(true);
    return true;
}

bool EditorViewportController::setScale(const double horizontal, const double vertical,
                                        const QPointF &anchor) {
    if (!std::isfinite(horizontal) || !std::isfinite(vertical) || horizontal <= 0.0 ||
        vertical <= 0.0) {
        return false;
    }
    const auto anchorTick = sceneXToTick(m_offsetX + anchor.x());
    const auto anchorUnit = sceneYToUnit(m_offsetY + anchor.y());
    m_scaleX = horizontal;
    m_scaleY = vertical;
    normalize(true);
    m_offsetX = tickToSceneX(anchorTick) - anchor.x();
    m_offsetY = unitToSceneY(anchorUnit) - anchor.y();
    normalize(false);
    notify(true);
    return true;
}

bool EditorViewportController::centerAt(const double tick, const double unit) {
    if (!std::isfinite(tick) || !std::isfinite(unit))
        return false;
    m_offsetX = tickToSceneX(tick) - m_viewportSize.width() * 0.5;
    m_offsetY = unitToSceneY(unit) - m_viewportSize.height() * 0.5;
    normalize(false);
    notify(false);
    return true;
}

bool EditorViewportController::setStartTick(const double tick) {
    if (!std::isfinite(tick))
        return false;
    m_offsetX = tickToSceneX(tick);
    normalize(false);
    notify(false);
    return true;
}

void EditorViewportController::scrollBy(const QPointF &deltaPixels) {
    m_offsetX += deltaPixels.x();
    m_offsetY += deltaPixels.y();
    normalize(false);
    notify(false);
}

void EditorViewportController::zoomHorizontal(const double wheelDelta, const double anchorX) {
    if (qFuzzyIsNull(wheelDelta))
        return;
    const auto factor =
        wheelDelta > 0.0 ? 1.0 + 0.4 * wheelDelta / 120.0 : 1.0 / (1.0 + 0.4 * -wheelDelta / 120.0);
    setScale(m_scaleX * factor, m_scaleY, {anchorX, m_viewportSize.height() * 0.5});
}

void EditorViewportController::zoomVertical(const double wheelDelta, const double anchorY) {
    if (qFuzzyIsNull(wheelDelta))
        return;
    const auto factor =
        wheelDelta > 0.0 ? 1.0 + 0.3 * wheelDelta / 120.0 : 1.0 / (1.0 + 0.3 * -wheelDelta / 120.0);
    setScale(m_scaleX, m_scaleY * factor, {m_viewportSize.width() * 0.5, anchorY});
}

double EditorViewportController::horizontalScale() const {
    return m_scaleX;
}

double EditorViewportController::verticalScale() const {
    return m_scaleY;
}

double EditorViewportController::startTick() const {
    return sceneXToTick(m_offsetX);
}

double EditorViewportController::endTick() const {
    return sceneXToTick(m_offsetX + m_viewportSize.width());
}

double EditorViewportController::topUnit() const {
    return sceneYToUnit(m_offsetY);
}

double EditorViewportController::bottomUnit() const {
    return sceneYToUnit(m_offsetY + m_viewportSize.height());
}

double EditorViewportController::leftMarginPx() const {
    return m_leftMarginPx;
}

double EditorViewportController::horizontalOffset() const {
    return m_offsetX;
}

double EditorViewportController::verticalOffset() const {
    return m_offsetY;
}

QRectF EditorViewportController::visibleSceneRect() const {
    return {QPointF(m_offsetX, m_offsetY), m_viewportSize};
}

QSizeF EditorViewportController::viewportSize() const {
    return m_viewportSize;
}

double EditorViewportController::tickToSceneX(const double tick) const {
    return (tick - m_startTick) * pixelsPerTick() + m_leftMarginPx;
}

double EditorViewportController::sceneXToTick(const double x) const {
    return m_startTick + (x - m_leftMarginPx) / std::max(0.000001, pixelsPerTick());
}

double EditorViewportController::unitToSceneY(const double unit) const {
    return unit * m_unitHeight * m_scaleY;
}

double EditorViewportController::sceneYToUnit(const double y) const {
    return y / std::max(0.000001, m_unitHeight * m_scaleY);
}

QPointF EditorViewportController::viewportToScene(const QPointF &position) const {
    return position + QPointF(m_offsetX, m_offsetY);
}

void EditorViewportController::normalize(const bool scaleChanged) {
    Q_UNUSED(scaleChanged);
    m_scaleX = std::clamp(m_scaleX, effectiveMinimumScaleX(), m_maxScaleX);
    m_scaleY = std::clamp(m_scaleY, effectiveMinimumScaleY(), m_maxScaleY);
    m_offsetX = std::clamp(m_offsetX, 0.0, std::max(0.0, contentWidth() - m_viewportSize.width()));
    m_offsetY =
        std::clamp(m_offsetY, 0.0, std::max(0.0, contentHeight() - m_viewportSize.height()));
}

void EditorViewportController::notify(const bool emitScaleChanged) {
    if (emitScaleChanged)
        emit scaleChanged(m_scaleX, m_scaleY);
    emit timeRangeChanged(startTick(), endTick());
    emit verticalRangeChanged(topUnit(), bottomUnit());
    emit scrollChanged(m_offsetX, m_offsetY);
    emit viewportChanged();
}

double EditorViewportController::effectiveMinimumScaleX() const {
    if (!m_fillX || m_viewportSize.width() <= 0.0 || m_endTick <= m_startTick)
        return m_minScaleX;
    const auto unscaledWidth = (m_endTick - m_startTick) * m_pixelsPerQuarterNote /
                               static_cast<double>(AppGlobal::ticksPerQuarterNote);
    return std::clamp(m_viewportSize.width() / std::max(1.0, unscaledWidth), m_minScaleX,
                      m_maxScaleX);
}

double EditorViewportController::effectiveMinimumScaleY() const {
    if (!m_fillY || m_viewportSize.height() <= 0.0 || m_unitCount <= 0.0)
        return m_minScaleY;
    return std::clamp(m_viewportSize.height() / std::max(1.0, m_unitCount * m_unitHeight),
                      m_minScaleY, m_maxScaleY);
}

double EditorViewportController::contentWidth() const {
    return std::max(0.0, (m_endTick - m_startTick) * pixelsPerTick()) + m_leftMarginPx;
}

double EditorViewportController::contentHeight() const {
    return std::max(0.0, m_unitCount * m_unitHeight * m_scaleY);
}

double EditorViewportController::pixelsPerTick() const {
    return m_pixelsPerQuarterNote * m_scaleX / AppGlobal::ticksPerQuarterNote;
}
