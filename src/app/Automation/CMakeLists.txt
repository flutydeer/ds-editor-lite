file(GLOB_RECURSE _edit_action_sources CONFIGURE_DEPENDS
    "../Controller/Actions/AppModel/Track/*.cpp"
    "../Controller/Actions/AppModel/Clip/*.cpp"
    "../Controller/Actions/AppModel/Note/*.cpp"
    "../Controller/Actions/AppModel/Param/*.cpp"
    "../Controller/Actions/AppModel/SpeakerMix/*.cpp"
)

add_library(EditorAutomationCore STATIC
    ApplicationAutomationFacade.cpp
    AudioExportAutomationFacade.cpp
    AutomationTypes.cpp
    Public/AdmissionController.cpp
    Public/AutomationFileGuard.cpp
    AutomationTaskManager.cpp
    IdempotencyStore.cpp
    DocumentSession.cpp
    DocumentSessionResolver.cpp
    DocumentAutomationFacade.cpp
    SingleWindowContext.cpp
    AutomationDispatcher.cpp
    CommandCommitter.cpp
    DocumentObjectResolver.cpp
    EditorAutomationFacade.cpp
    ExtractionAutomationFacade.cpp
    FileAutomationFacade.cpp
    HistoryAutomationFacade.cpp
    InferenceAutomationFacade.cpp
    NoteAutomationFacade.cpp
    NoteTransfer.cpp
    PackageAutomationFacade.cpp
    ParameterAutomationFacade.cpp
    PlaybackAutomationFacade.cpp
    PresetAutomationFacade.cpp
    SettingsAutomationFacade.cpp
    ProjectAutomationDtos.cpp
    ProjectAutomationFacade.cpp
    TaskAutomationFacade.cpp
    TimelineAutomationFacade.cpp
    CoreRuntime.cpp
    ../Controller/Actions/AppModel/MasterControl/EditMasterControlAction.cpp
    ../Controller/Actions/AppModel/MasterControl/MasterControlActions.cpp
    ../Controller/Actions/AppModel/ImportProjectActions.cpp
    ../Controller/Actions/AppModel/Import/BatchImportActions.cpp
    ../Controller/Actions/AppModel/Tempo/EditTemposAction.cpp
    ../Controller/Actions/AppModel/Tempo/TempoActions.cpp
    ../Controller/Actions/AppModel/TimeSignature/EditTimeSignaturesAction.cpp
    ../Controller/Actions/AppModel/TimeSignature/TimeSignatureActions.cpp
    ../Model/AppModel/SingingClipPhonemeNormalizer.cpp
    ../Model/Utils/ParamUtils.cpp
    ../Model/ClipboardDataModel/ClipsInfo.cpp
    ../Model/ClipboardDataModel/NotesParamsInfo.cpp
    ../Model/ClipboardDataModel/ParameterCurvesJson.cpp
    ../Model/AppOptions/Options/FillLyricOption.cpp
    ../Modules/FillLyric/Utils/LyricRuleAutomationUtils.cpp
    ../Modules/FillLyric/Utils/TaggerRuleOrder.cpp
    ../Modules/FillLyric/Utils/TextSplitter.cpp
    ../Modules/FillLyric/Utils/TextTagger.cpp
    ../Modules/FillLyric/Utils/SplitLyric.cpp
    ../Modules/FillLyric/Utils/LrcTools/LrcDecoder.cpp
    ../UI/Views/TrackEditor/AudioClipDragState.cpp
    ../UI/Views/ClipEditor/DrawCurveEditUtils.cpp
    ../UI/Views/ClipEditor/CurveTransform/CurveTransformSession.cpp
    ../UI/Views/ClipEditor/AnchorEditor/AnchorEditUtils.cpp
    ../UI/Views/ClipEditor/AnchorEditor/AnchorEditController.cpp
    ../UI/Views/ClipEditor/CurveRenderUtils.cpp
    ../UI/Views/ClipEditor/PianoRoll/PitchDisplayStrategy.cpp
    ../Controller/PianoRollNoteCommit.cpp
    ../UI/Utils/OverlappingHandleResolver.cpp
    ../UI/Utils/SpeakerMixUtils.cpp
    ${_edit_action_sources}
)
set_target_properties(EditorAutomationCore PROPERTIES AUTOMOC ON)
target_compile_features(EditorAutomationCore PUBLIC cxx_std_20)
target_compile_definitions(EditorAutomationCore PRIVATE NOMINMAX QT_MESSAGELOGCONTEXT)
target_include_directories(EditorAutomationCore PUBLIC
    ..
    "${LITE_BUILD_INCLUDE_DIR}"
)
target_link_libraries(EditorAutomationCore PUBLIC
    lite::ADT lite::History lite::MusicBase lite::ProjectModel Qt6::Core re2::re2
    PRIVATE lite::PackageManager
)
