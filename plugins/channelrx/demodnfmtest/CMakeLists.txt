project(nfmtest)

set(nfmtest_SOURCES
	nfmtestdemod.cpp
    nfmtestdemodsettings.cpp
    nfmtestdemodwebapiadapter.cpp
    nfmtestplugin.cpp
    nfmdemodreport.cpp
    nfmdemodsink.cpp
    nfmdemodbaseband.cpp
)

set(nfmtest_HEADERS
	nfmtestdemod.h
    nfmtestdemodsettings.h
    nfmtestdemodwebapiadapter.h
    nfmtestplugin.h
    nfmdemodreport.h
    nfmdemodsink.h
    nfmdemodbaseband.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(nfmtest_SOURCES
        ${nfmtest_SOURCES}
        nfmtestdemodgui.cpp

	nfmtestdemodgui.ui
    )
    set(nfmtest_HEADERS
        ${nfmtest_HEADERS}
        nfmtestdemodgui.h
    )

    set(TARGET_NAME demodnfmtest)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodnfmtestsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${nfmtest_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
