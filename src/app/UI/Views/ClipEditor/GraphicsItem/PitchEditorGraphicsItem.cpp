//
// Created by fluty on 2024/1/25.
//

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>

#include "PitchEditorGraphicsItem.h"

#include "Model/Clip.h"
#include "Utils/MathUtils.h"
#include "Global/ClipEditorGlobal.h"
#include "Model/Curve.h"

using namespace ClipEditorGlobal;

PitchEditorGraphicsItem::PitchEditorGraphicsItem() {
    setBackgroundColor(Qt::transparent);
    // loadOpensvipPitchParam();
}
PitchEditorGraphicsItem::EditMode PitchEditorGraphicsItem::editMode() const {
    return m_editMode;
}
void PitchEditorGraphicsItem::loadOpensvipPitchParam() {
    auto loadProjectFile = [](const QString &filename, QJsonObject *jsonObj) {
        QFile loadFile(filename);
        if (!loadFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Failed to open project file";
            return false;
        }
        QByteArray allData = loadFile.readAll();
        loadFile.close();
        QJsonParseError err;
        QJsonDocument json = QJsonDocument::fromJson(allData, &err);
        if (err.error != QJsonParseError::NoError)
            return false;
        *jsonObj = json.object();
        return true;
    };

    auto filename = "E:/Test/Param/小小opensvip.json";
    QJsonObject obj;
    if (loadProjectFile(filename, &obj)) {
        auto arrTracks = obj.value("TrackList").toArray();
        auto firstTrack = arrTracks.first().toObject();
        auto objEditedParams = firstTrack.value("EditedParams").toObject();
        auto objPitch = objEditedParams.value("Pitch").toObject();
        auto arrPointList = objPitch.value("PointList").toArray();
        for (const auto valPoint : arrPointList) {
            auto arrPoint = valPoint.toArray();
            auto pos = arrPoint.first().toInt();
            auto val = arrPoint.last().toInt();
            auto pair = std::make_pair(pos, val);
            m_opensvipPitchParam.append(pair);
        }
    }
}
void PitchEditorGraphicsItem::loadOriginal(const OverlapableSerialList<DrawCurve> &curves) {
    OverlapableSerialList<Curve> source;
    OverlapableSerialList<Curve> target;
    for (const auto curve : curves)
        source.add(curve);

    SingingClip::copyCurves(source, target);

    m_drawCurvesOriginal.clear();
    for (const auto curve : target) {
        if (curve->type() == Curve::Draw)
            m_drawCurvesOriginal.add(dynamic_cast<DrawCurve *>(curve));
    }
    update();
}
void PitchEditorGraphicsItem::loadEdited(const OverlapableSerialList<DrawCurve> &curves) {
    // qDebug() << "PitchEditorGraphicsItem::loadEdited count:" << curves.count();
    OverlapableSerialList<Curve> source;
    OverlapableSerialList<Curve> target;
    for (const auto curve : curves)
        source.add(curve);

    SingingClip::copyCurves(source, target);

    m_drawCurvesEdited.clear();
    for (const auto curve : target) {
        if (curve->type() == Curve::Draw)
            m_drawCurvesEdited.add(dynamic_cast<DrawCurve *>(curve));
    }
    update();
}
const OverlapableSerialList<DrawCurve> &PitchEditorGraphicsItem::editedCurves() const {
    return m_drawCurvesEdited;
}
void PitchEditorGraphicsItem::setEditMode(const EditMode &mode) {
    if (mode == Off)
        setTransparentForMouseEvents(true);
    else {
        setTransparentForMouseEvents(false);
    }
    m_editMode = mode;
}
void PitchEditorGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                    QWidget *widget) {
    OverlayGraphicsItem::paint(painter, option, widget);

    // painter->setRenderHint(QPainter::Antialiasing, false);
    auto noAntialiasingThreshold = 0.8;
    auto hideThreshold = 0.4;
    auto fadeLength = 0.1;
    if (scaleX() < hideThreshold)
        return;
    if (scaleX() < noAntialiasingThreshold)
        painter->setRenderHint(QPainter::Antialiasing, false);

    QPen pen;
    pen.setWidthF(1.8);
    auto colorAlpha =
        scaleX() < hideThreshold + fadeLength ? 255 * (scaleX() - hideThreshold) / fadeLength : 255;

    // if (m_opensvipPitchParam.isEmpty())
    //     return;
    // drawOpensvipPitchParam(painter);

    if (m_drawCurvesOriginal.count() > 0) {
        pen.setColor(QColor(127, 127, 127, static_cast<int>(colorAlpha)));
        painter->setPen(pen);
        drawHandDrawCurves(painter, m_drawCurvesOriginal);
    }

    if (m_drawCurvesEdited.count() > 0) {
        pen.setColor(QColor(255, 255, 255, static_cast<int>(colorAlpha)));
        painter->setPen(pen);
        drawHandDrawCurves(painter, m_drawCurvesEdited);
    }
}
void PitchEditorGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    // OverlayGraphicsItem::mousePressEvent(event);
    auto scenePos = event->scenePos().toPoint();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    auto pitch = static_cast<int>(sceneYToPitch(scenePos.y()));
    // qDebug() << "PitchEditorGraphicsItem::mousePressEvent tick:" << tick << "pitch:" << pitch;

    auto curve = curveAt(tick);
    if (curve) {
        m_editingCurve = curve;
        m_drawCurveEditType = EditExistCurve;
        qDebug() << "Edit exist curve" << curve->id() << curve->start();
    } else {
        m_editingCurve = new DrawCurve;
        m_editingCurve->setStart(tick);
        m_editingCurve->appendValue(pitch);
        m_drawCurveEditType = CreateNewCurve;
        m_drawCurvesEdited.add(m_editingCurve);
        qDebug() << "New curve added" << m_editingCurve->id();
    }
    m_mouseDownPos = QPoint(tick, pitch);
    m_prevPos = m_mouseDownPos;
}
void PitchEditorGraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    // OverlayGraphicsItem::mouseMoveEvent(event);
    m_mouseMoved = true;
    auto scenePos = event->scenePos();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    auto pitch = static_cast<int>(sceneYToPitch(scenePos.y()));
    auto curPos = QPoint(tick, pitch);
    // qDebug() << "Draw curve at" << curPos;

    int startTick;
    int endTick;
    if (m_prevPos.x() < curPos.x()) {
        startTick = m_prevPos.x();
        endTick = curPos.x();
    } else {
        endTick = m_prevPos.x();
        startTick = curPos.x();
    }
    drawLine(m_prevPos, curPos, m_editingCurve);
    auto overlappedCurves = curvesIn(startTick, endTick);
    if (!overlappedCurves.isEmpty()) {
        for (auto curve : overlappedCurves) {
            if (curve == m_editingCurve)
                continue;

            m_editingCurve->mergeWith(*curve);
            m_drawCurvesEdited.remove(curve);
            // delete curve;
        }
    }
    m_prevPos = curPos;
    update();
}
void PitchEditorGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    // OverlayGraphicsItem::mouseReleaseEvent(event);
    qDebug() << "PitchEditorGraphicsItem::mouseReleaseEvent moved:" << m_mouseMoved;
    if (!m_mouseMoved) {
        if (m_drawCurveEditType == CreateNewCurve) {
            m_drawCurvesEdited.remove(m_editingCurve);
            delete m_editingCurve;
            qDebug() << "New curve removed";
        }
        m_editingCurve = nullptr;
        m_drawCurveEditType = None;
    } else {
        emit editCompleted();
        qDebug() << "PitchEditorGraphicsItem emit editCompleted";
    }

    m_mouseMoved = false;
}
void PitchEditorGraphicsItem::updateRectAndPos() {
    auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}
double PitchEditorGraphicsItem::startTick() const {
    return sceneXToTick(visibleRect().left());
}
double PitchEditorGraphicsItem::endTick() const {
    return sceneXToTick(visibleRect().right());
}
double PitchEditorGraphicsItem::sceneXToTick(double x) const {
    return 480 * x / scaleX() / pixelsPerQuarterNote;
}
double PitchEditorGraphicsItem::tickToSceneX(double tick) const {
    return tick * scaleX() * pixelsPerQuarterNote / 480;
}
double PitchEditorGraphicsItem::sceneXToItemX(double x) const {
    return mapFromScene(QPointF(x, 0)).x();
}
double PitchEditorGraphicsItem::tickToItemX(double tick) const {
    return sceneXToItemX(tickToSceneX(tick));
}
double PitchEditorGraphicsItem::pitchToSceneY(double pitch) const {
    return (12700 - pitch + 50) * scaleY() * noteHeight / 100;
}
double PitchEditorGraphicsItem::sceneYToItemY(double y) const {
    return mapFromScene(QPointF(0, y)).y();
}
double PitchEditorGraphicsItem::pitchToItemY(double pitch) const {
    return sceneYToItemY(pitchToSceneY(pitch));
}
double PitchEditorGraphicsItem::sceneYToPitch(double y) const {
    return -(y * 100 / noteHeight / scaleY() - 12700 - 50);
}
DrawCurve *PitchEditorGraphicsItem::curveAt(double tick) {
    for (const auto curve : m_drawCurvesEdited)
        if (curve->start() <= tick && curve->endTick() > tick)
            return curve;
    return nullptr;
}
QList<DrawCurve *> PitchEditorGraphicsItem::curvesIn(int startTick, int endTick) {
    QList<DrawCurve *> result;
    ProbeLine line = ProbeLine();
    line.setStart(startTick);
    line.setEndTick(endTick);
    for (const auto curve : m_drawCurvesEdited) {
        if (curve->isOverlappedWith(&line))
            result.append(curve);
    }
    return result;
}
void PitchEditorGraphicsItem::drawOpensvipPitchParam(QPainter *painter) {
    QPainterPath path;
    bool firstPoint = true;
    int prevPos = 0;
    int prevValue = 0;
    for (const auto &point : m_opensvipPitchParam) {
        auto pos = std::get<0>(point) - 480 * 3; // opensvip's "feature"
        auto value = std::get<1>(point);

        if (pos < startTick()) {
            prevPos = pos;
            prevValue = value;
            continue;
        }

        if (firstPoint) {
            path.moveTo(tickToItemX(prevPos), pitchToItemY(prevValue));
            path.lineTo(tickToItemX(pos), pitchToItemY(value));
            firstPoint = false;
        } else
            path.lineTo(tickToItemX(pos), pitchToItemY(value));

        if (pos > endTick()) {
            path.lineTo(tickToItemX(pos), pitchToItemY(value));
            break;
        }
    }
    painter->drawPath(path);
}
void PitchEditorGraphicsItem::drawHandDrawCurves(QPainter *painter,
                                                 const OverlapableSerialList<DrawCurve> &curves) {
    auto drawCurve = [&](DrawCurve *curve) {
        QPainterPath path;
        int start = curve->start();
        auto firstValue = curve->values().first();
        auto firstPos = QPointF(tickToItemX(start), pitchToItemY(firstValue));
        path.moveTo(firstPos);
        painter->drawText(firstPos, QString("#%1").arg(curve->id()));
        for (int i = 0; i < curve->values().count(); i++) {
            auto pos = start + curve->step * i;
            auto value = curve->values().at(i);
            if (pos < startTick())
                continue;
            if (pos > endTick())
                break;

            path.lineTo(tickToItemX(pos), pitchToItemY(value));
        }
        painter->drawPath(path);
    };

    for (const auto curve : curves) {
        if (curve->endTick() < startTick())
            continue;
        if (curve->start() > endTick())
            break;

        drawCurve(curve);
    }
}
void PitchEditorGraphicsItem::drawLine(const QPoint &p1, const QPoint &p2, DrawCurve *curve) {
    if (p1.x() == p2.x())
        return;

    auto valueAt = [](const QPoint &p1, const QPoint &p2, int x) {
        int x1 = p1.x();
        int y1 = p1.y();
        int x2 = p2.x();
        int y2 = p2.y();
        double dx = x2 - x1;
        double dy = y2 - y1;
        double ratio = dy / dx;
        return static_cast<int>(y1 + (x - x1) * ratio);
    };

    QPoint startPoint;
    QPoint endPoint;
    if (p1.x() < p2.x()) {
        startPoint = p1;
        endPoint = p2;
    } else {
        startPoint = p2;
        endPoint = p1;
    }
    auto line = DrawCurve(-1);
    auto start = startPoint.x();
    line.setStart(start);
    int linePointCount = (endPoint.x() - startPoint.x()) / curve->step;
    for (int i = 0; i < linePointCount; i++) {
        auto tick = start + i * curve->step;
        auto value = valueAt(startPoint, endPoint, tick);
        line.appendValue(value);
    }
    curve->overlayMergeWith(line);
}