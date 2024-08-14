//
// Created by fluty on 24-8-14.
//

#include "ParamUtils.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

QList<std::pair<int, int>> ParamUtils::loadOpensvipPitchParam(const QString &filename) {
    QList<std::pair<int, int>> result;
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
            result.append(pair);
        }
    }

    return result;
}