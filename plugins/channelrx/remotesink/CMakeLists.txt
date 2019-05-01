project(remotesink)

if (HAS_SSSE3)
    message(STATUS "RemoteSink: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "RemoteSink: use Neon SIMD" )
else()
    message(STATUS "RemoteSink: Unsupported architecture")
    return()
endif()

set(remotesink_SOURCES
	remotesink.cpp
	remotesinksettings.cpp
	remotesinkthread.cpp
	remotesinkplugin.cpp
)

set(remotesink_HEADERS
	remotesink.h
	remotesinksettings.h
	remotesinkthread.h
	remotesinkplugin.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(remotesink_SOURCES
        ${remotesink_SOURCES}
        remotesinkgui.cpp

	remotesinkgui.ui
    )
    set(remotesink_HEADERS
        ${remotesink_HEADERS}
        remotesinkgui.h
    )

    set(TARGET_NAME remotesink)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER "plugins")
else()
    set(TARGET_NAME remotesinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER "pluginssrv")
endif()

add_library(${TARGET_NAME} SHARED
	${remotesink_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        ${CM256CC_LIBRARIES}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_LIB_DIR}/${INSTALL_FOLDER}/channelrx)
