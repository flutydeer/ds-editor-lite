#include "AudioLoadHint.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace AudioLoadHint {
    namespace {
        // The moving arc spins at the same rate as the ProgressIndicator control, so a loading clip
        // and a running task in the task view read as the same kind of activity
        constexpr double speedDegreesPerSecond = 270.0;
        constexpr double minSweepDegrees = 30.0;
        constexpr double maxSweepDegrees = 90.0;
        constexpr double pulseHertz = 0.2;
        // One segment every few degrees keeps the small circle round without flooding the vertex
        // buffer of the RHI backend
        constexpr double degreesPerSegment = 6.0;
        constexpr int trackSegments = 60;

        [[nodiscard]] QPointF pointAt(const QPointF &center, const double radius,
                                      const double degreesClockwise) {
            const auto radians = degreesClockwise * std::numbers::pi / 180.0;
            return center + QPointF(radius * std::sin(radians), -radius * std::cos(radians));
        }
    }

    Layout layout(const QRectF &labelRect, const double textWidth, const double scale) {
        const auto diameter = ringDiameter * scale;
        const auto gap = ringTextGap * scale;
        const auto blockWidth = diameter + gap + textWidth;
        const auto left = labelRect.center().x() - blockWidth * 0.5;
        return {
            .ringCenter = QPointF(left + diameter * 0.5, labelRect.center().y()),
            .ringRadius = (diameter - ringStrokeWidth * scale) * 0.5,
            .textLeft = left + diameter + gap,
        };
    }

    qint64 clockMs() {
        static const QElapsedTimer timer = [] {
            QElapsedTimer started;
            started.start();
            return started;
        }();
        return timer.elapsed();
    }

    double headDegrees(const qint64 elapsedMs) {
        const auto seconds = static_cast<double>(elapsedMs) / 1000.0;
        return std::fmod(seconds * speedDegreesPerSecond, 360.0);
    }

    double sweepDegrees(const qint64 elapsedMs) {
        const auto seconds = static_cast<double>(elapsedMs) / 1000.0;
        return minSweepDegrees +
               (maxSweepDegrees - minSweepDegrees) *
                   (0.5 + 0.5 * std::sin(seconds * pulseHertz * 2.0 * std::numbers::pi));
    }

    QVector<QPointF> arcCenterline(const QPointF &center, const double radius,
                                   const qint64 elapsedMs) {
        const auto sweep = sweepDegrees(elapsedMs);
        const auto head = headDegrees(elapsedMs);
        const auto segments = std::max(2, static_cast<int>(std::ceil(sweep / degreesPerSegment)));
        QVector<QPointF> points;
        points.reserve(segments + 1);
        for (int index = 0; index <= segments; ++index) {
            const auto offset = sweep * static_cast<double>(index) / static_cast<double>(segments);
            points.append(pointAt(center, radius, head - sweep + offset));
        }
        return points;
    }

    QVector<QPointF> trackCenterline(const QPointF &center, const double radius) {
        QVector<QPointF> points;
        points.reserve(trackSegments + 1);
        for (int index = 0; index <= trackSegments; ++index) {
            points.append(
                pointAt(center, radius,
                        360.0 * static_cast<double>(index) / static_cast<double>(trackSegments)));
        }
        return points;
    }

    QString textFor(const AppGlobal::AudioClipLoadState state, const QString &errorMessage) {
        switch (state) {
            case AppGlobal::AudioClipLoadState::Loading:
                // Same context as the hint on the clip in the track editor, so one translation
                // entry serves both views
                return QCoreApplication::translate("AudioClipView", "Loading...");
            case AppGlobal::AudioClipLoadState::Missing: {
                auto text = QCoreApplication::translate("AudioClipView", "File missing");
                if (!errorMessage.isEmpty())
                    text += ": " + errorMessage;
                return text;
            }
            case AppGlobal::AudioClipLoadState::Loaded:
                break;
        }
        return {};
    }

    QColor colorFor(const AppGlobal::AudioClipLoadState state, const QColor &clipColor) {
        auto color = clipColor;
        if (state == AppGlobal::AudioClipLoadState::Missing)
            color.setAlphaF(color.alphaF() * 0.5);
        return color;
    }

    void paintHint(QPainter *painter, const QRectF &labelRect, const QString &text,
                   const QColor &color, const bool animated) {
        if (text.isEmpty() || labelRect.isEmpty())
            return;
        const QFontMetricsF metrics(painter->font());
        if (!animated) {
            painter->save();
            painter->setPen(color);
            painter->drawText(labelRect, text, QTextOption(Qt::AlignCenter));
            painter->restore();
            return;
        }

        const auto textWidth = metrics.horizontalAdvance(text);
        const auto block = layout(labelRect, textWidth, 1.0);
        const auto stroke = ringStrokeWidth;

        const auto toPath = [](const QVector<QPointF> &points) {
            QPainterPath path;
            if (points.isEmpty())
                return path;
            path.moveTo(points.constFirst());
            for (qsizetype index = 1; index < points.size(); ++index)
                path.lineTo(points.at(index));
            return path;
        };

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setBrush(Qt::NoBrush);

        auto trackColor = color;
        trackColor.setAlphaF(trackColor.alphaF() * trackAlpha);
        painter->setPen(QPen(trackColor, stroke, Qt::SolidLine, Qt::RoundCap));
        painter->drawPath(toPath(trackCenterline(block.ringCenter, block.ringRadius)));

        painter->setPen(QPen(color, stroke, Qt::SolidLine, Qt::RoundCap));
        painter->drawPath(toPath(arcCenterline(block.ringCenter, block.ringRadius, clockMs())));

        painter->setPen(color);
        const auto textRect =
            QRectF(block.textLeft, labelRect.top(), textWidth, labelRect.height());
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        painter->restore();
    }
}
