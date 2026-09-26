add_library(AutomationTestSupport STATIC ../TestSupport/ProjectSnapshot.cpp)
target_include_directories(AutomationTestSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AutomationTestSupport PUBLIC EditorAutomationCore lite::ProjectConverters
    PRIVATE Qt6::Test)
