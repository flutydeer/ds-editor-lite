//
// Created by fluty on 24-3-13.
//

#define CLASS_NAME "AppOptions"

#include "AppOptions.h"

#include "Utils/JsonUtils.h"
#include "Utils/Log.h"

#include <QStandardPaths>
#include <QDir>

AppOptions::AppOptions(QObject *parent) : QObject(parent) {
    QDir configDir(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first());
    if (!configDir.exists()) {
        if (configDir.mkpath("."))
            Log::d(CLASS_NAME, "Successfully created config directory");
        else
            Log::e(CLASS_NAME, "Failed to create config directory");
    } else
        Log::d(CLASS_NAME, "Config directory already exists");
    m_configPath = configDir.absoluteFilePath(fileName);
    Log::i(CLASS_NAME, "Config path: " + m_configPath);
    QJsonObject obj;
    if (QFile::exists(m_configPath))
        if (JsonUtils::load(m_configPath, obj)) {
            m_generalOption.load(obj.value(m_generalOption.key()).toObject());
            m_audioOption.load(obj.value(m_audioOption.key()).toObject());
            m_appearanceOption.load(obj.value(m_appearanceOption.key()).toObject());
            m_languageOption.load(obj.value(m_languageOption.key()).toObject());
            m_fillLyricOption.load(obj.value(m_fillLyricOption.key()).toObject());
        }
    saveAndNotify();
}

QString AppOptions::configPath() const {
    return m_configPath;
}

bool AppOptions::saveAndNotify() {
    QJsonObject obj;
    obj.insert(m_generalOption.key(), m_generalOption.value());
    obj.insert(m_audioOption.key(), m_audioOption.value());
    obj.insert(m_appearanceOption.key(), m_appearanceOption.value());
    obj.insert(m_languageOption.key(), m_languageOption.value());
    obj.insert(m_fillLyricOption.key(), m_fillLyricOption.value());

    notifyOptionsChanged();
    return JsonUtils::save(m_configPath, obj);
}

GeneralOption *AppOptions::general() {
    return &m_generalOption;
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

LanguageOption *AppOptions::language() {
    return &m_languageOption;
}

FillLyricOption *AppOptions::fillLyric() {
    return &m_fillLyricOption;
}