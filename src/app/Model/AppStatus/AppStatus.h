#ifndef APPSTATUS_H
#define APPSTATUS_H

#include "EditorPreview.h"

#define appStatus AppStatus::instance()

#include <lite/ProjectModel/AppModel/LoopSettings.h>
#include <lite/ADT/Property.h>
#include <lite/Core/Singleton.h>

#include <QObject>
#include <QRectF>
#include <QVector>
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
    Property<QString>
        languageModuleError; // R6/TD-8: 语言引擎启动失败的具体原因，运行期状态不持久化
    Property<ModuleStatus> inferEngineEnvStatus = ModuleStatus::Unknown;
    Property<ModuleStatus> packageModuleStatus = ModuleStatus::Unknown;

    // Main Window
    Property<bool> trackPanelCollapsed = false;
    Property<bool> bottomPanelCollapsed = false;

    // Project
    Property<int> pianoRollQuantize = 16;
    Property<bool> pianoRollQuantizeEnabled = true;
    Property<int> projectEditableLength = AppGlobal::ticksPerWholeNote * 100;
    Property<int> selectedTrackIndex = -1;
    Property<int> activeClipId = -1;
    Property<QList<int>> selectedNotes;
    Property<QList<int>> selectedClips;
    Property<int> primarySelectedNoteId = -1;
    Property<int> primarySelectedClipId = -1;
    Property<EditObjectType> currentEditObject = EditObjectType::None;
    // Piano roll viewport (tick range x, keyIndex range y); null rect = invalid
    Property<QRectF> pianoRollVisibleRect;

    // 钢琴卷帘编辑进行中（未提交）的音符实时几何；空列表 = 无编辑
    using NoteEditPreview = EditorPreview::Note;

    Property<QVector<NoteEditPreview>> pianoRollNoteEditPreview;

    // 钢琴卷帘擦除中（未提交）被移除的音符 id；空列表 = 无擦除
    Property<QList<int>> pianoRollNoteErasePreview;

    // Loop
    Property<LoopSettings> loopSettings;

    // Playback viewport
    Property<bool> trackAutoPageTurnEnabled = true;
    Property<bool> trackAutoPageTurnAvailable = true;
    Property<bool> pianoRollAutoPageTurnEnabled = true;
    Property<bool> pianoRollAutoPageTurnAvailable = true;

signals:
    // Modules
    void moduleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);
    void languageModuleErrorChanged(const QString &error);

    // Main Window
    void trackPanelCollapseStateChanged(bool collapsed);
    void bottomPanelCollapseStateChanged(bool collapsed);

    // Project
    void pianoRollQuantizeChanged(int quantize);
    void pianoRollQuantizeEnabledChanged(bool enabled);
    void projectEditableLengthChanged(int newLength);
    void selectedTrackIndexChanged(int trackIndex);
    void activeClipIdChanged(int newId);
    void pianoRollVisibleRectChanged(const QRectF &rect);
    void pianoRollNoteEditPreviewChanged(const QVector<NoteEditPreview> &preview);
    void pianoRollNoteErasePreviewChanged(const QList<int> &noteIds);
    void noteSelectionChanged(const QList<int> &selectedNotes);
    void clipSelectionChanged(const QList<int> &selectedClips);
    void editingChanged(AppStatus::EditObjectType type);

    // Loop
    void loopSettingsChanged(const LoopSettings &settings);

    // Playback viewport
    void trackAutoPageTurnEnabledChanged(bool enabled);
    void trackAutoPageTurnAvailabilityChanged(bool available);
    void pianoRollAutoPageTurnEnabledChanged(bool enabled);
    void pianoRollAutoPageTurnAvailabilityChanged(bool available);
};

#endif // APPSTATUS_H
