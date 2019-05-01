project(remoteoutput)

if (HAS_SSSE3)
    message(STATUS "RemoteFEC: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "RemoteFEC: use Neon SIMD" )
else()
    message(STATUS "RemoteFEC: Unsupported architecture")
    return()
endif()

set(remoteoutput_SOURCES
	remoteoutput.cpp
	remoteoutputplugin.cpp
	remoteoutputsettings.cpp
	remoteoutputthread.cpp
        udpsinkfec.cpp
        udpsinkfecworker.cpp
)

set(remoteoutput_HEADERS
	remoteoutput.h
	remoteoutputplugin.h
	remoteoutputsettings.h
	remoteoutputthread.h
        udpsinkfec.h
        udpsinkfecworker.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
        ${CM256CC_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(remoteoutput_SOURCES
        ${remoteoutput_SOURCES}
        remoteoutputgui.cpp

        remoteoutputgui.ui
    )
    set(remoteoutput_HEADERS
        ${remoteoutput_HEADERS}
        remoteoutputgui.h
    )

    set(TARGET_NAME outputremote)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputremotesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${remoteoutput_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${CM256CC_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})