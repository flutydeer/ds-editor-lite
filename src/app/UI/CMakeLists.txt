find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg GuiPrivate)
find_path(INTERVAL_TREE_INCLUDE_DIRS "interval-tree/interval_tree.hpp" REQUIRED)

add_library(EditorGuiCore STATIC
    ../Controller/EditorViewController.cpp
    ../Controller/UndoRedoController.cpp
    ../Model/AppStatus/AppStatus.cpp
    Controls/TwoLevelComboBox.cpp
    Dialogs/Base/DialogTitleBar.cpp
    Utils/WaveformRenderUtils.cpp
    Views/ClipEditor/PianoRoll/PianoRollCoord.cpp
    Views/Common/EdgeAutoScroller.cpp
    Views/Common/EditorGlyphAtlas.cpp
    Views/Common/EditorRhiGeometry.cpp
    Views/Common/EditorRhiScrollBarController.cpp
    Views/Common/EditorViewportAnimation.cpp
    Views/Common/EditorViewportController.cpp
    Views/Common/TimeGraphicsScene.cpp
    Views/TrackEditor/SingingClipPreviewLayout.cpp
    ../Resources/lite_res.qrc
)
set_target_properties(EditorGuiCore PROPERTIES AUTOMOC OFF AUTORCC ON)
target_compile_features(EditorGuiCore PUBLIC cxx_std_20)
target_compile_definitions(EditorGuiCore PRIVATE NOMINMAX QT_MESSAGELOGCONTEXT)
target_include_directories(EditorGuiCore PUBLIC
    ..
    "${LITE_BUILD_INCLUDE_DIR}"
    ${INTERVAL_TREE_INCLUDE_DIRS}
)
target_link_libraries(EditorGuiCore PUBLIC
    EditorAutomationCore EditorPreferencesCore
    lite::GUI lite::PackageManager
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::GuiPrivate
)

# Separate moc objects let component consumers link without the application-bound controllers.
qt6_wrap_cpp(EditorGuiCore
    ../Controller/EditorViewController.h
    ../Controller/UndoRedoController.h
    ../Model/AppStatus/AppStatus.h
    Controls/TwoLevelComboBox.h
    Dialogs/Base/DialogTitleBar.h
    Views/Common/EdgeAutoScroller.h
    Views/Common/EditorRhiScrollBarController.h
    Views/Common/EditorViewportController.h
    Views/Common/TimeGraphicsScene.h
)
