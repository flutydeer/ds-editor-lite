//
// Created by fluty on 2026/5/4.
//

#include "SpeakerMixEditorView.h"

#include "UI/Utils/TrackColorPalette.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QPainter>

SpeakerMixEditorView::SpeakerMixEditorView() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    setTransparentMouseEvents(false);

    auto *palette = TrackColorPalette::instance();
    m_speakers = {
        {QStringLiteral("spk1"), palette->baseColor(0), palette->clipBackgroundTransparent(0)},
        {QStringLiteral("spk2"), palette->baseColor(1), palette->clipBackgroundTransparent(1)},
        {QStringLiteral("spk3"), palette->baseColor(2), palette->clipBackgroundTransparent(2)},
    };

    m_keyframes = {
        {0,    {0.33, 0.33}, SpeakerMixKeyframe::Hermite},
        {480,  {0.60, 0.20}, SpeakerMixKeyframe::Hermite},
        {960,  {0.10, 0.40}, SpeakerMixKeyframe::Linear },
        {1440, {0.33, 0.33}, SpeakerMixKeyframe::Hermite},
    };
}

void SpeakerMixEditorView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                 QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (m_speakers.isEmpty() || m_keyframes.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    drawStackedArea(painter);
    drawKeyframeDots(painter);
}

void SpeakerMixEditorView::updateRectAndPos() {
    const auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

QList<double> SpeakerMixEditorView::interpolateWeights(const double tick) const {
    if (m_keyframes.isEmpty())
        return {};

    const int n = m_speakers.size();

    if (tick <= m_keyframes.first().tick) {
        QList<double> result = m_keyframes.first().weights;
        double sum = 0;
        for (auto w : result)
            sum += w;
        result.append(1.0 - sum);
        return result;
    }

    if (tick >= m_keyframes.last().tick) {
        QList<double> result = m_keyframes.last().weights;
        double sum = 0;
        for (auto w : result)
            sum += w;
        result.append(1.0 - sum);
        return result;
    }

    int idx = 0;
    for (int i = 0; i < m_keyframes.size() - 1; i++) {
        if (tick >= m_keyframes[i].tick && tick < m_keyframes[i + 1].tick) {
            idx = i;
            break;
        }
    }

    const auto &kf0 = m_keyframes[idx];
    const auto &kf1 = m_keyframes[idx + 1];
    const double t =
        static_cast<double>(tick - kf0.tick) / static_cast<double>(kf1.tick - kf0.tick);

    QList<double> result;
    result.reserve(n);
    double sum = 0;
    for (int i = 0; i < n - 1; i++) {
        const double w = kf0.weights[i] + t * (kf1.weights[i] - kf0.weights[i]);
        result.append(w);
        sum += w;
    }
    result.append(1.0 - sum);
    return result;
}

void SpeakerMixEditorView::drawStackedArea(QPainter *painter) const {
    const double viewWidth = visibleRect().width();
    const double viewHeight = visibleRect().height();
    const int n = m_speakers.size();
    const double padding = 2.0;

    if (viewWidth <= 0 || viewHeight <= 0)
        return;

    const double areaTop = padding;
    const double areaHeight = viewHeight - 2 * padding;

    QList<QPainterPath> fillPaths;
    QList<QPainterPath> borderPaths;
    QList<QList<double>> topEdges;
    fillPaths.reserve(n);
    borderPaths.reserve(n - 1);
    topEdges.reserve(n);
    for (int i = 0; i < n; i++) {
        fillPaths.append(QPainterPath());
        topEdges.append(QList<double>());
    }
    for (int i = 0; i < n - 1; i++)
        borderPaths.append(QPainterPath());

    const double step = 1.0;
    const int sampleCount = static_cast<int>(viewWidth / step) + 1;

    for (int s = 0; s <= sampleCount; s++) {
        const double localX = s * step;
        const double sceneX = localX + visibleRect().left();
        const double tick = sceneXToTick(sceneX);
        const auto weights = interpolateWeights(tick);

        double cumulative = 0;
        for (int i = 0; i < n; i++) {
            cumulative += weights[i];
            const double y = areaTop + areaHeight * (1.0 - cumulative);
            topEdges[i].append(y);
        }
    }

    for (int i = 0; i < n; i++) {
        QPainterPath &path = fillPaths[i];

        const double bottomY0 =
            (i == 0) ? (areaTop + areaHeight) : topEdges[i - 1].first();
        path.moveTo(0, bottomY0);

        for (int s = 0; s <= sampleCount; s++) {
            const double localX = s * step;
            const double bottomY =
                (i == 0) ? (areaTop + areaHeight) : topEdges[i - 1][s];
            path.lineTo(localX, bottomY);
        }

        for (int s = sampleCount; s >= 0; s--) {
            const double localX = s * step;
            path.lineTo(localX, topEdges[i][s]);
        }

        path.closeSubpath();
    }

    for (int i = 0; i < n - 1; i++) {
        QPainterPath &border = borderPaths[i];
        border.moveTo(0, topEdges[i].first());
        for (int s = 1; s <= sampleCount; s++) {
            const double localX = s * step;
            border.lineTo(localX, topEdges[i][s]);
        }
    }

    for (int i = 0; i < n; i++) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_speakers[i].fillColor);
        painter->drawPath(fillPaths[i]);
    }

    const QPen borderPen(QColor(220, 220, 220, 200), 1.0);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(borderPen);
    for (int i = 0; i < n - 1; i++)
        painter->drawPath(borderPaths[i]);
}

void SpeakerMixEditorView::drawKeyframeDots(QPainter *painter) const {
    const double viewHeight = visibleRect().height();
    const int n = m_speakers.size();
    const double padding = 2.0;
    const double areaTop = padding;
    const double areaHeight = viewHeight - 2 * padding;
    constexpr double dotRadius = 4.0;

    for (const auto &kf : m_keyframes) {
        const double localX = tickToItemX(kf.tick);

        if (localX < -dotRadius || localX > visibleRect().width() + dotRadius)
            continue;

        painter->setPen(QPen(QColor(220, 220, 220, 160), 1.0));
        painter->drawLine(QPointF(localX, areaTop), QPointF(localX, areaTop + areaHeight));

        const auto weights = interpolateWeights(kf.tick);

        double cumulative = 0;
        for (int i = 0; i < n - 1; i++) {
            cumulative += weights[i];
            const double y = areaTop + areaHeight * (1.0 - cumulative);

            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255));
            painter->drawEllipse(QPointF(localX, y), dotRadius, dotRadius);
        }
    }
}
