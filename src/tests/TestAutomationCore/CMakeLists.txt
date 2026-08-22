project(TestAutomationCore)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Automation/AutomationTypes.cpp
        ../../app/Automation/OperationCatalog.cpp
        ../../app/Automation/IdempotencyStore.cpp
        ../../app/Automation/DocumentSession.cpp
        ../../app/Automation/DocumentSessionResolver.cpp
        ../../app/Automation/SingleWindowContext.cpp
        ../../app/Automation/AutomationDispatcher.cpp
        ../../app/Automation/CommandCommitter.cpp
        ../../app/Automation/EditorAutomationFacade.cpp
        ../../app/Automation/HistoryAutomationFacade.cpp
        ../../app/Automation/TimelineAutomationFacade.cpp
        ../../app/Automation/CoreRuntime.cpp
        ../../app/Controller/Actions/AppModel/MasterControl/EditMasterControlAction.cpp
        ../../app/Controller/Actions/AppModel/MasterControl/MasterControlActions.cpp
        ../../app/Controller/Actions/AppModel/Tempo/EditTemposAction.cpp
        ../../app/Controller/Actions/AppModel/Tempo/TempoActions.cpp
        ../../app/Controller/Actions/AppModel/TimeSignature/EditTimeSignaturesAction.cpp
        ../../app/Controller/Actions/AppModel/TimeSignature/TimeSignatureActions.cpp
        ../../app/Model/AppModel/SingingClipPhonemeNormalizer.cpp
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
