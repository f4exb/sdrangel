project(modcw)

set(modcw_SOURCES
    cwmod.cpp
    cwmodplugin.cpp
    cwmodsettings.cpp
    cwmodbaseband.cpp
    cwmodsource.cpp
    cwmodwebapiadapter.cpp
)

set(modcw_HEADERS
    cwmod.h
    cwmodplugin.h
    cwmodsettings.h
    cwmodbaseband.h
    cwmodsource.h
    cwmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modcw_SOURCES
        ${modcw_SOURCES}
        cwmodgui.cpp
    )
    set(modcw_HEADERS
        ${modcw_HEADERS}
        cwmodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}modcw)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modcwsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modcw_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME CWModPlugin ${modcw_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
)

# Copy the built DLL/shared library into the top-level BuiltPLUGIN directory
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/BuiltPLUGIN"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${TARGET_NAME}>"
        "${CMAKE_SOURCE_DIR}/BuiltPLUGIN/"
    COMMENT "Copying ${TARGET_NAME} to BuiltPLUGIN/"
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols on Windows
if(WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb
            CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}>
            CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER})
endif()
