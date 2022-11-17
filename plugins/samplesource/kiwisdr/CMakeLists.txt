project(kiwisdr)

set(kiwisdr_SOURCES
	kiwisdrinput.cpp
	kiwisdrplugin.cpp
	kiwisdrworker.cpp
    kiwisdrsettings.cpp
    kiwisdrwebapiadapter.cpp
)

set(kiwisdr_HEADERS
	kiwisdrinput.h
	kiwisdrplugin.h
	kiwisdrworker.h
    kiwisdrsettings.h
    kiwisdrwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(kiwisdr_SOURCES
        ${kiwisdr_SOURCES}
        kiwisdrgui.cpp
        kiwisdrgui.ui
    )
    set(kiwisdr_HEADERS
        ${kiwisdr_HEADERS}
        kiwisdrgui.h
    )

	set(TARGET_NAME inputkiwisdr)
	set(TARGET_LIB "Qt::Widgets")
	set(TARGET_LIB_GUI "sdrgui")
	set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputkiwisdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${kiwisdr_SOURCES}
)

target_link_libraries(${TARGET_NAME}
	Qt::Core
	Qt::WebSockets
	${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
