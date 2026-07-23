//
// Created by OrangeCat on 24-8-24.
//

#ifndef APPSTATUS_H
#define APPSTATUS_H

#define appStatus AppStatus::instance()

#include "Model/AppModel/LoopSettings.h"
#include <lite/ADT/Property.h>
#include "Utils/Singleton.h"

#include <QObject>
#include "Global/AppGlobal.h"

class AppStatus : public QObject {
    Q_OBJECT

private:
    explicit AppStatus(QObject *parent = nullptr);
    ~AppStatus() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(AppStatus)
    Q_DISABLE_COPY_MOVE(AppStatus)

public:
    enum class ModuleType { Audio, Language, Inference, Package };
    enum class ModuleStatus { Ready, Loading, Error, Unknown };
    enum class EditObjectType { None, Clip, Note, Phoneme, Param };

    // Modules
    Property<ModuleStatus> languageModuleStatus = ModuleStatus::Unknown;
    Property<QString> languageModuleError; // R6/TD-8: 语言引擎启动失败的具体原因，运行期状态不持久化
    Property<ModuleStatus> inferEngineEnvStatus = ModuleStatus::Unknown;
    Property<ModuleStatus> packageModuleStatus = ModuleStatus::Unknown;

    // Main Window
    Property<bool> trackPanelCollapsed = false;
    Property<bool> bottomPanelCollapsed = false;

    // Project
    Property<int> pianoRollQuantize = 16;
    Property<int> projectEditableLength = AppGlobal::ticksPerWholeNote * 100;
    Property<int> selectedTrackIndex = -1;
    Property<int> activeClipId = -1;
    Property<QList<int>> selectedNotes;
    Property<QList<int>> selectedClips;
    Property<EditObjectType> currentEditObject = EditObjectType::None;

    // Loop
    Property<LoopSettings> loopSettings;

signals:
    // Modules
    void moduleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);
    void languageModuleErrorChanged(const QString &error);

    // Main Window
    void trackPanelCollapseStateChanged(bool collapsed);
    void bottomPanelCollapseStateChanged(bool collapsed);

    // Project
    void pianoRollQuantizeChanged(int quantize);
    void projectEditableLengthChanged(int newLength);
    void selectedTrackIndexChanged(int trackIndex);
    void activeClipIdChanged(int newId);
    void noteSelectionChanged(const QList<int> &selectedNotes);
    void clipSelectionChanged(const QList<int> &selectedClips);
    void editingChanged(AppStatus::EditObjectType type);

    // Loop
    void loopSettingsChanged(const LoopSettings &settings);
};

#endif // APPSTATUS_H
