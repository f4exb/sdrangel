project(rtlsdr2)

set(rtlsdr2_SOURCES
    rtlsdr2input.cpp
    rtlsdr2plugin.cpp
    rtlsdr2settings.cpp
    rtlsdr2webapiadapter.cpp
    rtlsdr2thread.cpp
)

set(rtlsdr2_HEADERS
    rtlsdr2input.h
    rtlsdr2plugin.h
    rtlsdr2settings.h
    rtlsdr2webapiadapter.h
    rtlsdr2thread.h
)
include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBRTLSDR_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(rtlsdr2_SOURCES
        ${rtlsdr2_SOURCES}
        rtlsdr2gui.cpp

        rtlsdr2gui.ui
    )
    set(rtlsdr2_HEADERS
        ${rtlsdr2_HEADERS}
        rtlsdr2gui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputrtlsdr2)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputrtlsdr2srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${rtlsdr2_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RTLSDR2Plugin)
    target_sources(${TARGET_NAME} PRIVATE ${rtlsdr2_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(LIBUSB_DEPENDS)
    add_dependencies(${TARGET_NAME} ${LIBUSB_DEPENDS})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBRTLSDR_LIBRARIES}
        ${LIBUSB_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
