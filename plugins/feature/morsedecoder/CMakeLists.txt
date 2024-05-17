project(morsedecoder)

set(morsedecoder_SOURCES
	morsedecoder.cpp
    morsedecodersettings.cpp
    morsedecoderplugin.cpp
    morsedecoderworker.cpp
    morsedecoderwebapiadapter.cpp
)

set(morsedecoder_HEADERS
	morsedecoder.h
    morsedecodersettings.h
    morsedecoderplugin.h
    morsedecoderworker.h
    morsedecoderwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${GGMORSE_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(morsedecoder_SOURCES
        ${morsedecoder_SOURCES}
        morsedecodergui.cpp
        morsedecodergui.ui
    )
    set(morsedecoder_HEADERS
        ${morsedecoder_HEADERS}
        morsedecodergui.h
    )

    set(TARGET_NAME featuremorsedecoder)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featuremorsedecodersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${morsedecoder_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    ${GGMORSE_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
