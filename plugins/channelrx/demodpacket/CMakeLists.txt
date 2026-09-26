project(demodpacket)

set(demodpacket_SOURCES
    packetdemod.cpp
    packetdemodsettings.cpp
    packetdemodbaseband.cpp
    packetdemodsink.cpp
    packetdemodplugin.cpp
    packetdemodwebapiadapter.cpp
)

set(demodpacket_HEADERS
    packetdemod.h
    packetdemodsettings.h
    packetdemodbaseband.h
    packetdemodsink.h
    packetdemodg3ruh.h
    packetdemodcore.h
    packetdemodframer.h
    packetdemodtonecorrelator.h
    packetdemodplugin.h
    packetdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodpacket_SOURCES
        ${demodpacket_SOURCES}
        packetdemodgui.cpp
        packetdemodgui.ui
    )
    set(demodpacket_HEADERS
        ${demodpacket_HEADERS}
        packetdemodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodpacket)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodpacketsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodpacket_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME PacketDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demodpacket_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

option(PACKETDEMOD_BUILD_TESTS "Build Packet Demod sink integration tests" OFF)
if(PACKETDEMOD_BUILD_TESTS AND SERVER_MODE)
    add_executable(packetdemod_sink_test
        tests/test_sink.cpp
        packetdemodsink.cpp
        packetdemodsettings.cpp)
    target_include_directories(packetdemod_sink_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(packetdemod_sink_test PRIVATE Qt::Core sdrbase ${TARGET_NAME})
    option(PACKETDEMOD_TEST_SANITIZERS "Sanitize the Packet Demod sink test" OFF)
    if(PACKETDEMOD_TEST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(packetdemod_sink_test PRIVATE -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer)
        target_link_options(packetdemod_sink_test PRIVATE -fsanitize=address,undefined,float-cast-overflow)
    endif()
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
