project(remotesource)

if (HAS_SSSE3)
    message(STATUS "RemoteSource: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "RemoteSource: use Neon SIMD" )
else()
    message(STATUS "RemoteSource: Unsupported architecture")
    return()
endif()

set(remotesource_SOURCES
	remotesource.cpp
	remotesourcethread.cpp
	remotesourceplugin.cpp
	remotesourcesettings.cpp
)

set(remotesource_HEADERS
	remotesource.h
	remotesourcethread.h
	remotesourceplugin.h
	remotesourcesettings.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(remotesource_SOURCES
        ${remotesource_SOURCES}
        remotesourcegui.cpp

        remotesourcegui.ui
    )
    set(remotesource_HEADERS
        ${remotesource_HEADERS}
        remotesourcegui.h
    )

    set(TARGET_NAME remotesource)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER "plugins")
else()
    set(TARGET_NAME remotesourcesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER "pluginssrv")
endif()

add_library(${TARGET_NAME} SHARED
	${remotesource_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
        Qt5::Network
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${CM256CC_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_LIB_DIR}/${INSTALL_FOLDER}/channeltx)
