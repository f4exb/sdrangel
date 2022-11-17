project(rtlsdr)

set(rtlsdr_SOURCES
    rtlsdrinput.cpp
    rtlsdrplugin.cpp
    rtlsdrsettings.cpp
    rtlsdrwebapiadapter.cpp
    rtlsdrthread.cpp
)

set(rtlsdr_HEADERS
    rtlsdrinput.h
    rtlsdrplugin.h
    rtlsdrsettings.h
    rtlsdrwebapiadapter.h
    rtlsdrthread.h
)
include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBRTLSDR_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(rtlsdr_SOURCES
        ${rtlsdr_SOURCES}
        rtlsdrgui.cpp

        rtlsdrgui.ui
    )
    set(rtlsdr_HEADERS
        ${rtlsdr_HEADERS}
        rtlsdrgui.h
    )

    set(TARGET_NAME inputrtlsdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputrtlsdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${rtlsdr_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBRTLSDR_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
