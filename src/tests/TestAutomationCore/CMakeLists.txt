project(TestAutomationCore)

file(GLOB_RECURSE _automation_edit_action_sources CONFIGURE_DEPENDS
    "../../app/Controller/Actions/AppModel/Track/*.cpp"
    "../../app/Controller/Actions/AppModel/Clip/*.cpp"
    "../../app/Controller/Actions/AppModel/Note/*.cpp"
    "../../app/Controller/Actions/AppModel/Param/*.cpp"
    "../../app/Controller/Actions/AppModel/SpeakerMix/*.cpp"
)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Automation/AutomationTypes.cpp
        ../../app/Automation/AutomationTaskManager.cpp
        ../../app/Automation/OperationCatalog.cpp
        ../../app/Automation/IdempotencyStore.cpp
        ../../app/Automation/DocumentSession.cpp
        ../../app/Automation/DocumentSessionResolver.cpp
        ../../app/Automation/DocumentAutomationFacade.cpp
        ../../app/Automation/SingleWindowContext.cpp
        ../../app/Automation/AutomationDispatcher.cpp
        ../../app/Automation/CommandCommitter.cpp
        ../../app/Automation/DocumentObjectResolver.cpp
        ../../app/Automation/EditorAutomationFacade.cpp
        ../../app/Automation/HistoryAutomationFacade.cpp
        ../../app/Automation/InferenceAutomationFacade.cpp
    ../../app/Automation/NoteAutomationFacade.cpp
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
        ${_automation_edit_action_sources}
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    lite::ADT
    lite::History
    lite::MusicBase
    lite::ProjectModel
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
