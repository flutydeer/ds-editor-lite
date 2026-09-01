project(${LITE_CONNECTOR_EXECUTABLE_BASENAME} VERSION ${LITE_VERSION})

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

set(_core_sources
    BootstrapWatcher.cpp
    BootstrapWatcher.h
    ConnectorOptions.cpp
    ConnectorOptions.h
    ConnectorRuntime.cpp
    ConnectorRuntime.h
    DownstreamMcpServer.cpp
    DownstreamMcpServer.h
    ExposurePolicy.cpp
    ExposurePolicy.h
    StdioTransport.cpp
    StdioTransport.h
    UpstreamMcpClient.cpp
    UpstreamMcpClient.h
    ../app/Bootstrap/SingleInstanceIdentity.cpp
    ../app/Bootstrap/SingleInstanceProtocol.cpp
)

add_library(DsConnectorCore STATIC ${_core_sources})
add_library(lite::DsConnectorCore ALIAS DsConnectorCore)
set_target_properties(DsConnectorCore PROPERTIES AUTOMOC ON)
target_compile_features(DsConnectorCore PUBLIC cxx_std_20)
target_include_directories(DsConnectorCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../app
    ${LITE_BUILD_INCLUDE_DIR}
)
target_link_libraries(DsConnectorCore PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    lite::AutomationWire
)

lite_add_tool(${PROJECT_NAME}
    SOURCES main.cpp
    INCLUDE_PRIVATE . ../app
    FEATURES_PRIVATE cxx_std_20
    QT_LINKS_PRIVATE Core Network
)
target_link_libraries(${PROJECT_NAME} PRIVATE lite::DsConnectorCore)

qm_add_win_rc_enhanced(${PROJECT_NAME}
    NAME "${LITE_CONNECTOR_PRODUCT_NAME}"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "${LITE_CONNECTOR_PRODUCT_NAME}"
    COPYRIGHT "${LITE_PRODUCT_COPYRIGHT}"
    COMPANY "${LITE_PRODUCT_PUBLISHER}"
    INTERNAL_NAME "${LITE_CONNECTOR_EXECUTABLE_BASENAME}"
    ORIGINAL_FILENAME "${LITE_CONNECTOR_EXECUTABLE_BASENAME}.exe"
)

if(LITE_INSTALL)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${LITE_INSTALL_RUNTIME_DIR})
endif()
