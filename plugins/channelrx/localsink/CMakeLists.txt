project(localsink)

set(localsink_SOURCES
  localsink.cpp
  localsinksettings.cpp
  localsinkthread.cpp
  localsinkplugin.cpp
)

set(localsink_HEADERS
	localsink.h
	localsinksettings.h
	localsinkthread.h
	localsinkplugin.h
        )

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIR}
    )

if(NOT SERVER_MODE)
  set(localsink_SOURCES
    ${localsink_SOURCES}
    localsinkgui.cpp

    localsinkgui.ui
    )
  set(localsink_HEADERS
    ${localsink_HEADERS}
    localsinkgui.h
    )

  set(TARGET_NAME localsink)
  set(TARGET_LIB "Qt5::Widgets")
  set(TARGET_LIB_GUI "sdrgui")
  set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
  set(TARGET_NAME localsinksrv)
  set(TARGET_LIB "")
  set(TARGET_LIB_GUI "")
  set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
  ${localsink_SOURCES}
  )

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        ${CM256CC_LIBRARIES}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
