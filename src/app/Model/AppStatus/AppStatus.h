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
    enum class ModuleType { Audio, Language, Inference };
    enum class ModuleStatus { Ready, Loading, Error, Unknown };
    enum class EditObjectType { None, Clip, Note, Phoneme, Param };

    explicit AppStatus();

    // Modules
    Property<ModuleStatus> languageModuleStatus = ModuleStatus::Unknown;
    Property<ModuleStatus> inferEngineEnvStatus = ModuleStatus::Unknown;

    // Main Window
    Property<bool> trackPanelCollapsed = false;
    Property<bool> clipPanelCollapsed = false;

    // Project
    Property<int> quantize = 16;
    Property<int> projectEditableLength = 1920 * 100;
    Property<int> selectedTrackIndex = -1;
    Property<int> activeClipId = -1;
    Property<QList<int>> selectedNotes;
    Property<EditObjectType> currentEditObject = EditObjectType::None;

signals:
    // Modules
    void moduleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);

    // Main Window
    void trackPanelCollapseStateChanged(bool collapsed);
    void clipPanelCollapseStateChanged(bool collapsed);

    // Project
    void quantizeChanged(int quantize);
    void projectEditableLengthChanged(int newLength);
    void selectedTrackIndexChanged(int trackIndex);
    void activeClipIdChanged(int newId);
    void noteSelectionChanged(const QList<int> &selectedNotes);
    void editingChanged(AppStatus::EditObjectType type);
};

#endif // APPSTATUS_H
