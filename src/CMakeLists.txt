include(conf.cmake)

lite_init_buildsystem(LITE)

# Product identity header, generated next to the library forwarding headers
# under LITE_BUILD_INCLUDE_DIR so it is consumed as <lite/ProductMetadata.h>
# (the LITE_PRODUCT_* values come from cmake/ProductMetadata.cmake).
file(MAKE_DIRECTORY "${LITE_BUILD_INCLUDE_DIR}/lite")
configure_file(
    ${LITE_SOURCE_DIR}/cmake/ProductMetadata.h.in
    "${LITE_BUILD_INCLUDE_DIR}/lite/ProductMetadata.h"
    @ONLY
)

add_subdirectory(3rdparty)

qm_find_qt(Core)
qm_find_qt(Widgets)
qm_find_qt(Svg)

# After Qt is found: project libraries may link Qt (e.g. lite::Support -> Qt::Core).
add_subdirectory(libs)

add_subdirectory(app)

if(LITE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

add_subdirectory(tools)

lite_finish_buildsystem()
