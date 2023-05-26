project(sigmffileinput)

set(sigmffileinput_SOURCES
	sigmffileinput.cpp
	sigmffileinputplugin.cpp
	sigmffileinputworker.cpp
    sigmffileinputsettings.cpp
    sigmffileinputwebapiadapter.cpp
)

set(sigmffileinput_HEADERS
	sigmffileinput.h
	sigmffileinputplugin.h
	sigmffileinputworker.h
    sigmffileinputsettings.h
    sigmffileinputwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBSIGMF_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(sigmffileinput_SOURCES
        ${sigmffileinput_SOURCES}
        sigmffileinputgui.cpp
        recordinfodialog.cpp
        sigmffileinputgui.ui
        recordinfodialog.ui
    )
    set(sigmffileinput_HEADERS
        ${sigmffileinput_HEADERS}
        sigmffileinputgui.h
        recordinfodialog.h
    )
    set(TARGET_NAME inputsigmffileinput)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputsigmffileinputsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${sigmffileinput_SOURCES}
)

if (NOT WIN32)
    link_directories(${Boost_LIBRARY_DIRS})
endif()

target_link_libraries(${TARGET_NAME}
    Boost::disable_autolinking
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
)
if(WIN32)
    target_link_libraries(sdrbase optimized ${LIBSIGMF_LIBRARIES})
    target_link_libraries(sdrbase debug ${LIBSIGMF_LIBRARIES_DEBUG})
else()
    target_link_libraries(sdrbase ${LIBSIGMF_LIBRARIES})
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
