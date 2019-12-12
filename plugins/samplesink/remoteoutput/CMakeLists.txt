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
    remoteoutputwebapiadapter.cpp
	remoteoutputthread.cpp
    udpsinkfec.cpp
    remoteoutputsender.cpp
    remoteoutputfifo.cpp
)

set(remoteoutput_HEADERS
	remoteoutput.h
	remoteoutputplugin.h
    remoteoutputsettings.h
    remoteoutputwebapiadapter.h
	remoteoutputthread.h
    udpsinkfec.h
    remoteoutputsender.h
    remoteoutputfifo.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
    ${Boost_INCLUDE_DIRS}
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

if(ENABLE_EXTERNAL_LIBRARIES)
    add_dependencies(${TARGET_NAME} cm256cc)
endif()

target_link_libraries(${TARGET_NAME}
    Qt5::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${CM256CC_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
