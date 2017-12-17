project(pluginssrv)

add_definitions(-DSERVER_MODE)

add_subdirectory(channelrx)
add_subdirectory(channeltx)
add_subdirectory(samplesink)
add_subdirectory(samplesource)
