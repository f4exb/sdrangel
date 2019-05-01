project(plugins)

include_directories(
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrgui
)

add_subdirectory(channelrx)
add_subdirectory(channeltx)
add_subdirectory(samplesource)
add_subdirectory(samplesink)
