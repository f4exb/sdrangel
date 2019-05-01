project(plugins)

include_directories(
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrgui
)

if (NOT SERVER_MODE)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BUILD_PLUGINS_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BUILD_PLUGINS_DIR})
else()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BUILD_PLUGINSSRV_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BUILD_PLUGINSSRV_DIR})
endif()

add_subdirectory(channelrx)
add_subdirectory(channeltx)
add_subdirectory(samplesource)
add_subdirectory(samplesink)
