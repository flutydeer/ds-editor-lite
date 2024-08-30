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
    Property<int> projectEditableLength = 1920 * 100;
    Property<int> activeClipId = -1;

signals:
    void moduleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);
    void projectEditableLengthChanged(int newLength);
};

#endif // APPSTATUS_H
