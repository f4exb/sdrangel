project(messagecenter)

set(messagecenter_SOURCES
    messagecenterarchive.cpp
    messagecenterengine.cpp
    messagecenter.cpp
    messagecentersettings.cpp
    messagecenterplugin.cpp
    messagecenterwebapiadapter.cpp
)

set(messagecenter_HEADERS
    messagecenterarchive.h
    messagecenterengine.h
    messagecenter.h
    messagecentersettings.h
    messagecenterplugin.h
    messagecenterwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/modemradiopaging
)

if(NOT SERVER_MODE)
    set(messagecenter_SOURCES
        ${messagecenter_SOURCES}
        messagecentergui.cpp
        messagecentercontacts.cpp
        messagecenter.qrc
    )
    set(messagecenter_HEADERS
        ${messagecenter_HEADERS}
        messagecentergui.h
        messagecentercontacts.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featuremessagecenter)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    if(Qt${QT_DEFAULT_MAJOR_VERSION}TextToSpeech_FOUND)
        list(APPEND TARGET_LIB_GUI Qt::TextToSpeech)
    endif()
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featuremessagecentersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${messagecenter_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME MessageCenterPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${messagecenter_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    Qt::Network
    ${TARGET_LIB}
    sdrbase
    modemradiopaging
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER})
endif()

# Self test of the engine, not built by default:
#   cmake --build <build dir> --target messagecenterenginetest
if(NOT SERVER_MODE)
    add_executable(messagecenterenginetest EXCLUDE_FROM_ALL
        test/messagecenterenginetest.cpp
        messagecenterarchive.cpp
        messagecenterengine.cpp
        messagecentersettings.cpp
    )
    target_include_directories(messagecenterenginetest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/modemradiopaging)
    target_link_libraries(messagecenterenginetest Qt::Core Qt::Network sdrbase modemradiopaging)

    # Self test of the archive (files by day and size, queries), not built by default:
    #   cmake --build <build dir> --target messagecenterarchivetest
    add_executable(messagecenterarchivetest EXCLUDE_FROM_ALL
        test/messagecenterarchivetest.cpp
        messagecenterarchive.cpp
        messagecenterengine.cpp
    )
    target_include_directories(messagecenterarchivetest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/modemradiopaging)
    target_link_libraries(messagecenterarchivetest Qt::Core Qt::Network sdrbase modemradiopaging)
endif()

# Self test of the table of paging messages (offscreen), not built by default:
#   cmake --build <build dir> --target pagingmessagetabletest
if(NOT SERVER_MODE)
    add_executable(pagingmessagetabletest EXCLUDE_FROM_ALL
        test/pagingmessagetabletest.cpp
    )
    target_link_libraries(pagingmessagetabletest Qt::Widgets sdrbase sdrgui)

    add_executable(pagingcomposertest EXCLUDE_FROM_ALL
        test/pagingcomposertest.cpp
    )
    target_link_libraries(pagingcomposertest Qt::Widgets sdrbase sdrgui)
endif()
