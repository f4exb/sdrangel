project(localinput)

set(localinput_SOURCES
  localinput.cpp
  localinputsettings.cpp
  localinputwebapiadapter.cpp
  localinputplugin.cpp
  )

set(localinput_HEADERS
  localinput.h
  localinputsettings.h
  localinputwebapiadapter.h
  localinputplugin.h
  )


include_directories(
  ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
  )

if(NOT SERVER_MODE)
  set(localinput_SOURCES
    ${localinput_SOURCES}
    localinputgui.cpp

    localinputgui.ui
    )
  set(localinput_HEADERS
    ${localinput_HEADERS}
    localinputgui.h
    )

  set(TARGET_NAME inputlocal)
  set(TARGET_LIB "Qt::Widgets")
  set(TARGET_LIB_GUI "sdrgui")
  set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
  set(TARGET_NAME inputlocalsrv)
  set(TARGET_LIB "")
  set(TARGET_LIB_GUI "")
  set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
  ${localinput_SOURCES}
  )

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
