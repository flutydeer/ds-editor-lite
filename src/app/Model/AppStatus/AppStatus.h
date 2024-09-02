//
// Created by OrangeCat on 24-8-24.
//

#ifndef APPSTATUS_H
#define APPSTATUS_H

#define appStatus AppStatus::instance()

#include "Utils/Property.h"
#include "Utils/Singleton.h"

#include <QObject>

class AppStatus : public QObject, public Singleton<AppStatus> {
    Q_OBJECT

public:
    enum class ModuleType { Audio, Language };
    enum class ModuleStatus { Ready, Loading, Error, Unknown };

    explicit AppStatus();

    // Modules
    Property<ModuleStatus> languageModuleStatus = ModuleStatus::Unknown;

    // Project
    Property<int> quantize = 16;
    Property<int> projectEditableLength = 1920 * 100;
    Property<int> selectedTrackIndex = -1;
    Property<int> activeClipId = -1;

signals:
    // Modules
    void moduleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);

    // Project
    void quantizeChanged(int quantize);
    void projectEditableLengthChanged(int newLength);
    void selectedTrackIndexChanged(int trackIndex);
    void activeClipIdChanged(int newId);
};

#endif // APPSTATUS_H
