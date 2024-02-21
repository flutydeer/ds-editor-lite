//
// Created by fluty on 2024/1/25.
//

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

#include "PitchEditorGraphicsItem.h"
#include "Views/ClipEditor/ClipEditorGlobal.h"

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
            opensvipPitchParam.append(pair);
        }
    }
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
    if (scaleX() < 0.6)
        return;

    QPen pen;
    pen.setWidthF(1);
    auto colorAlpha = scaleX() < 0.8 ? 255 * (scaleX() - 0.6) / (0.8 - 0.6) : 255;
    pen.setColor(QColor(255, 255, 255, static_cast<int>(colorAlpha)));
    painter->setPen(pen);

    if (opensvipPitchParam.isEmpty())
        return;
    drawOpensvipPitchParam(painter);
}
void PitchEditorGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    OverlayGraphicsItem::mousePressEvent(event);
}
void PitchEditorGraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    OverlayGraphicsItem::mouseMoveEvent(event);
}
void PitchEditorGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    OverlayGraphicsItem::mouseReleaseEvent(event);
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
void PitchEditorGraphicsItem::drawOpensvipPitchParam(QPainter *painter) {
    QPainterPath path;
    bool firstPoint = true;
    int prevPos = 0;
    int prevValue = 0;
    for (const auto &point : opensvipPitchParam) {
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