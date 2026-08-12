#define CLASS_NAME "AppOptions"

#include "AppOptions.h"

#include <lite/Support/JsonUtils.h>
#include <lite/Support/Log.h>

#include <QStandardPaths>
#include <QDir>

AppOptions::AppOptions(QObject *parent) : AppOptions({}, parent) {
}

AppOptions::AppOptions(QString configDirectory, QObject *parent) : QObject(parent) {
    const QDir configDir(configDirectory.isEmpty()
                             ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             : std::move(configDirectory));
    if (!configDir.exists()) {
        if (configDir.mkpath("."))
            Log::d(CLASS_NAME, "Successfully created config directory");
        else
            Log::e(CLASS_NAME, "Failed to create config directory");
    } else
        Log::d(CLASS_NAME, "Config directory already exists");
    m_configPath = configDir.absoluteFilePath(fileName);
    QJsonObject obj;
    if (QFile::exists(m_configPath))
        if (JsonUtils::load(m_configPath, obj)) {
            m_generalOption.load(obj.value(m_generalOption.key()).toObject());
            m_audioOption.load(obj.value(m_audioOption.key()).toObject());
            m_appearanceOption.load(obj.value(m_appearanceOption.key()).toObject());
            m_g2pLanguageOption.load(obj.value(m_g2pLanguageOption.key()).toObject());
            m_fillLyricOption.load(obj.value(m_fillLyricOption.key()).toObject());
            m_inferenceOption.load(obj.value(m_inferenceOption.key()).toObject());
            m_developerOption.load(obj.value(m_developerOption.key()).toObject());
            m_windowOption.load(obj.value(m_windowOption.key()).toObject());
        }
    QString revisionError;
    if (!FileRevisionUtils::capture(m_configPath, m_fileRevision, &revisionError))
        Log::e(CLASS_NAME, "Failed to capture config revision: " + revisionError);
}

AppOptions::~AppOptions() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppOptions)

QString AppOptions::configPath() const {
    return m_configPath;
}

bool AppOptions::saveAndNotify(const AppOptionsGlobal::Option option) {
    const QJsonObject obj{
        {m_generalOption.key(),     m_generalOption.value()    },
        {m_audioOption.key(),       m_audioOption.value()      },
        {m_appearanceOption.key(),  m_appearanceOption.value() },
        {m_g2pLanguageOption.key(), m_g2pLanguageOption.value()},
        {m_fillLyricOption.key(),   m_fillLyricOption.value()  },
        {m_inferenceOption.key(),   m_inferenceOption.value()  },
        {m_developerOption.key(),   m_developerOption.value()  },
        {m_windowOption.key(),      m_windowOption.value()     }
    };

    QString revisionError;
    if (!FileRevisionUtils::matchesCurrent(m_configPath, m_fileRevision, &revisionError)) {
        Log::e(CLASS_NAME,
               revisionError.isEmpty()
                   ? "Config file changed outside the application; refusing to overwrite"
                   : "Failed to verify config revision: " + revisionError);
        notifyOptionsChanged(option);
        return false;
    }

    const auto success = JsonUtils::save(m_configPath, obj);
    if (success && !FileRevisionUtils::capture(m_configPath, m_fileRevision, &revisionError))
        Log::e(CLASS_NAME, "Failed to refresh config revision: " + revisionError);
    notifyOptionsChanged(option);
    return success;
}

GeneralOption *AppOptions::general() {
    return &m_generalOption;
}

void AppOptions::notifyOptionsChanged(const AppOptionsGlobal::Option option) {
    emit optionsChanged(option);
}

AudioOption *AppOptions::audio() {
    return &m_audioOption;
}

AppearanceOption *AppOptions::appearance() {
    return &m_appearanceOption;
}

G2pLanguageOption *AppOptions::g2pLanguage() {
    return &m_g2pLanguageOption;
}

FillLyricOption *AppOptions::fillLyric() {
    return &m_fillLyricOption;
}

InferenceOption *AppOptions::inference() {
    return &m_inferenceOption;
}

DeveloperOption *AppOptions::developer() {
    return &m_developerOption;
}

WindowOption *AppOptions::window() {
    return &m_windowOption;
}
