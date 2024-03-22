//
// Created by fluty on 24-3-13.
//

#include "AppOptions.h"

#include <QStandardPaths>
#include <QDir>

#include "Utils/JsonUtils.h"

AppOptions::AppOptions(QObject *parent) : QObject(parent) {
    QDir configDir(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first());
    m_configPath = configDir.absoluteFilePath(fileName);
    QJsonObject obj;
    if (QFile::exists(m_configPath))
        if (JsonUtils::load(m_configPath, obj)) {
            m_audioOption.load(obj.value(m_audioOption.key()).toObject());
            m_appearanceOption.load(obj.value(m_appearanceOption.key()).toObject());
        }
}
bool AppOptions::saveAndNotify() {
    QJsonObject obj;
    obj.insert(m_audioOption.key(), m_audioOption.value());
    obj.insert(m_appearanceOption.key(), m_appearanceOption.value());

    notifyOptionsChanged();
    return JsonUtils::save(m_configPath, obj);
}
void AppOptions::notifyOptionsChanged() {
    emit optionsChanged();
}
AudioOption *AppOptions::audio() {
    return &m_audioOption;
}
AppearanceOption *AppOptions::appearance() {
    return &m_appearanceOption;
}