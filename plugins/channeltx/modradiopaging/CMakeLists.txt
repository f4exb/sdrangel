project(modradiopaging)

set(modradiopaging_SOURCES
    radiopagingmod.cpp
    radiopagingmodsettings.cpp
    radiopagingmodbaseband.cpp
    radiopagingmodsource.cpp
    radiopagingmodplugin.cpp
    radiopagingmodwebapiadapter.cpp
)

set(modradiopaging_HEADERS
    radiopagingmod.h
    radiopagingmodsettings.h
    radiopagingmodbaseband.h
    radiopagingmodsource.h
    radiopagingmodplugin.h
    radiopagingmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/modemradiopaging
)

if(NOT SERVER_MODE)
    set(modradiopaging_SOURCES
        ${modradiopaging_SOURCES}
        radiopagingmodgui.cpp
        radiopagingmodgui.ui
        radiopagingmodmessagemodel.cpp
    )
    set(modradiopaging_HEADERS
        ${modradiopaging_HEADERS}
        radiopagingmodgui.h
        radiopagingmodmessagemodel.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}modradiopaging)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modradiopagingsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modradiopaging_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RadioPagingModPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${modradiopaging_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    modemradiopaging
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Self test of the source (messages in, IQ out, received through the radio paging
# demodulator sink), not built by default:
#   cmake --build <build dir> --target radiopagingmodsourcetest
if(NOT SERVER_MODE AND ENABLE_CHANNELRX_DEMODRADIOPAGING)
    add_executable(radiopagingmodsourcetest EXCLUDE_FROM_ALL
        test/radiopagingmodsourcetest.cpp
        radiopagingmodsource.cpp
        radiopagingmodsettings.cpp
        ${CMAKE_SOURCE_DIR}/plugins/channelrx/demodradiopaging/radiopagingdemodsink.cpp
        ${CMAKE_SOURCE_DIR}/plugins/channelrx/demodradiopaging/radiopagingdemodsettings.cpp
    )
    target_include_directories(radiopagingmodsourcetest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/plugins/channelrx/demodradiopaging
    )
    target_link_libraries(radiopagingmodsourcetest Qt::Core sdrbase modemradiopaging)
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
