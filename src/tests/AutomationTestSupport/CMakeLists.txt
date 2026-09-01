file(GLOB_RECURSE _automation_edit_action_sources CONFIGURE_DEPENDS
    "../../app/Controller/Actions/AppModel/Track/*.cpp"
    "../../app/Controller/Actions/AppModel/Clip/*.cpp"
    "../../app/Controller/Actions/AppModel/Note/*.cpp"
    "../../app/Controller/Actions/AppModel/Param/*.cpp"
    "../../app/Controller/Actions/AppModel/SpeakerMix/*.cpp"
)

add_library(AutomationTestSupport STATIC
    ../../app/Automation/ApplicationAutomationFacade.cpp
    ../../app/Automation/AudioExportAutomationFacade.cpp
    ../../app/Automation/AutomationTypes.cpp
    ../../app/Automation/AutomationTaskManager.cpp
    ../../app/Automation/IdempotencyStore.cpp
    ../../app/Automation/DocumentSession.cpp
    ../../app/Automation/DocumentSessionResolver.cpp
    ../../app/Automation/DocumentAutomationFacade.cpp
    ../../app/Automation/SingleWindowContext.cpp
    ../../app/Automation/AutomationDispatcher.cpp
    ../../app/Automation/CommandCommitter.cpp
    ../../app/Automation/DocumentObjectResolver.cpp
    ../../app/Automation/EditorAutomationFacade.cpp
    ../../app/Automation/ExtractionAutomationFacade.cpp
    ../../app/Automation/FileAutomationFacade.cpp
    ../../app/Automation/HistoryAutomationFacade.cpp
    ../../app/Automation/InferenceAutomationFacade.cpp
    ../../app/Automation/NoteAutomationFacade.cpp
    ../../app/Automation/NoteTransfer.cpp
    ../../app/Automation/PackageAutomationFacade.cpp
    ../../app/Automation/ParameterAutomationFacade.cpp
    ../../app/Automation/PlaybackAutomationFacade.cpp
    ../../app/Automation/PresetAutomationFacade.cpp
    ../../app/Automation/SettingsAutomationFacade.cpp
    ../../app/Automation/ProjectAutomationDtos.cpp
    ../../app/Automation/ProjectAutomationFacade.cpp
    ../../app/Automation/TaskAutomationFacade.cpp
    ../../app/Automation/TimelineAutomationFacade.cpp
    ../../app/Automation/CoreRuntime.cpp
    ../../app/Controller/Actions/AppModel/MasterControl/EditMasterControlAction.cpp
    ../../app/Controller/Actions/AppModel/MasterControl/MasterControlActions.cpp
    ../../app/Controller/Actions/AppModel/ImportProjectActions.cpp
    ../../app/Controller/Actions/AppModel/Import/BatchImportActions.cpp
    ../../app/Controller/Actions/AppModel/Tempo/EditTemposAction.cpp
    ../../app/Controller/Actions/AppModel/Tempo/TempoActions.cpp
    ../../app/Controller/Actions/AppModel/TimeSignature/EditTimeSignaturesAction.cpp
    ../../app/Controller/Actions/AppModel/TimeSignature/TimeSignatureActions.cpp
    ../../app/Model/AppModel/SingingClipPhonemeNormalizer.cpp
    ../../app/Model/ClipboardDataModel/NotesParamsInfo.cpp
    ../../app/Modules/FillLyric/Utils/LyricRuleAutomationUtils.cpp
    ../../app/Modules/FillLyric/Utils/TaggerRuleOrder.cpp
    ../../app/UI/Views/TrackEditor/AudioClipDragState.cpp
    ${_automation_edit_action_sources}
)

set_target_properties(AutomationTestSupport PROPERTIES AUTOMOC ON)

target_include_directories(AutomationTestSupport PUBLIC
    ../../app
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(re2 CONFIG REQUIRED)

target_link_libraries(AutomationTestSupport PUBLIC
    lite::ADT
    lite::History
    lite::MusicBase
    lite::ProjectModel
    Qt${QT_VERSION_MAJOR}::Core
    re2::re2
)
