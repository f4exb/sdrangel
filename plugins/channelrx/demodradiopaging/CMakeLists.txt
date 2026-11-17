project(demodradiopaging)

set(demodradiopaging_SOURCES
    radiopagingdemod.cpp
    radiopagingdemodsettings.cpp
    radiopagingdemodbaseband.cpp
    radiopagingdemodsink.cpp
    radiopagingdemodplugin.cpp
    radiopagingdemodwebapiadapter.cpp
)

set(demodradiopaging_HEADERS
    radiopagingdemod.h
    radiopagingdemodsettings.h
    radiopagingdemodbaseband.h
    radiopagingdemodsink.h
    radiopagingdemodplugin.h
    radiopagingdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/modemradiopaging
)

if(NOT SERVER_MODE)
    set(demodradiopaging_SOURCES
        ${demodradiopaging_SOURCES}
        radiopagingdemodgui.cpp
        radiopagingdemodgui.ui
        radiopagingdemodnotificationdialog.cpp
        radiopagingdemodnotificationdialog.ui
    )
    set(demodradiopaging_HEADERS
        ${demodradiopaging_HEADERS}
        radiopagingdemodgui.h
        radiopagingdemodnotificationdialog.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodradiopaging)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    if(Qt${QT_DEFAULT_MAJOR_VERSION}TextToSpeech_FOUND)
        list(APPEND TARGET_LIB_GUI Qt::TextToSpeech)
    endif()
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodradiopagingsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodradiopaging_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RadioPagingDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demodradiopaging_SOURCES})
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

# Self test of the sink (IQ in, records out), not built by default:
#   cmake --build <build dir> --target radiopagingdemodsinktest
if(NOT SERVER_MODE)
    add_executable(radiopagingdemodsinktest EXCLUDE_FROM_ALL
        test/radiopagingdemodsinktest.cpp
        radiopagingdemodsink.cpp
        radiopagingdemodsettings.cpp
    )
    target_include_directories(radiopagingdemodsinktest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(radiopagingdemodsinktest Qt::Core sdrbase modemradiopaging)

    # Self test of the output (UDP datagram, log) and the settings, not built by default:
    #   cmake --build <build dir> --target radiopagingdemodparitytest
    add_executable(radiopagingdemodparitytest EXCLUDE_FROM_ALL
        test/radiopagingdemodparitytest.cpp
        radiopagingdemodsink.cpp
        radiopagingdemodsettings.cpp
    )
    target_include_directories(radiopagingdemodparitytest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(radiopagingdemodparitytest Qt::Core sdrbase modemradiopaging)

    # Decoding of a recording of FM discriminator audio (as multimon-ng reads) with the sink, not built by default:
    #   cmake --build <build dir> --target radiopagingaudiodecode
    add_executable(radiopagingaudiodecode EXCLUDE_FROM_ALL
        test/radiopagingaudiodecode.cpp
        radiopagingdemodsink.cpp
        radiopagingdemodsettings.cpp
    )
    target_include_directories(radiopagingaudiodecode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(radiopagingaudiodecode Qt::Core sdrbase modemradiopaging)
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
