project(dsddemod)

# Demodulator core and protocol decoders: no Qt, shared with the test harness
set(dsddemod_CORE_SOURCES
    dsdfrontend.cpp
    dsdsyncsearcher.cpp
    dsdsymbolclock.cpp
    dsdslicer.cpp
    dsdprotocoldmr.cpp
    dsdprotocoldstar.cpp
    dsdprotocolysf.cpp
    dsdprotocolnxdn.cpp
    dsdprotocoldpmr.cpp
    dsdprotocolp25.cpp
    dsdp25trunking.cpp
    dsdprotocolp25p2.cpp
    dsdprotocolm17.cpp
    dsddemodulator.cpp
    dsdcoherent.cpp
    dsdangel61hf.cpp
    dsdangel61gateway.cpp
    dsdangel61session.cpp
    dsdangel61vhf.cpp
    dsdangel61receiver.cpp
    dsdpackettext.cpp
    dsdvocodermbelib.cpp
    dsdvocoderopus.cpp
    dsdvocodervocos.cpp
    dsdvocodercodec2.cpp
)

set(dsddemod_CORE_HEADERS
    dsdfrontend.h
    dsdsyncsearcher.h
    dsdsymbolclock.h
    dsdslicer.h
    dsdprotocol.h
    dsdprotocoldmr.h
    dsdprotocoldstar.h
    dsdprotocolysf.h
    dsdprotocolnxdn.h
    dsdprotocoldpmr.h
    dsdprotocolp25.h
    dsdp25trunking.h
    dsdprotocolp25p2.h
    dsdprotocolm17.h
    dsddemodulator.h
    dsdcoherent.h
    dsdangel61hf.h
    dsdangel61gateway.h
    dsdangel61session.h
    dsdangel61vhf.h
    dsdangel61receiver.h
    dsdpackettext.h
    dsdvocodermbelib.h
    dsdvocoderopus.h
    dsdvocodervocos.h
    dsdvocodercodec2.h
)

set(dsddemod_SOURCES
    dsddemod.cpp
    dsddemodplugin.cpp
    dsddemodbaudrates.cpp
    dsddemodsettings.cpp
    dsddemodsink.cpp
    dsddemodbaseband.cpp
    dsddemodwebapiadapter.cpp
    ${dsddemod_CORE_SOURCES}
)

set(dsddemod_HEADERS
    dsddemod.h
    dsddemodplugin.h
    dsddemodbaudrates.h
    dsddemodsettings.h
    dsddemodsink.h
    dsddemodbaseband.h
    dsddemodwebapiadapter.h
    ${dsddemod_CORE_HEADERS}
)

include_directories(
    ${CODEC2_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBDSDCC_INCLUDE_DIR}
    ${LIBMBE_INCLUDE_DIR}
)

# The coherent detector's search is dot products of float arrays: let the compiler vectorise
# the reductions (nothing in the file relies on NaN or infinity semantics)
if(MSVC)
    set_source_files_properties(dsdcoherent.cpp PROPERTIES COMPILE_OPTIONS "/fp:fast")
else()
    set_source_files_properties(dsdcoherent.cpp PROPERTIES COMPILE_OPTIONS "-ffast-math")
endif()

if(NOT SERVER_MODE)
    set(dsddemod_SOURCES
        ${dsddemod_SOURCES}
        dsddemodgui.cpp
        dsdstatustextdialog.cpp
        dsddemodgui.ui
        dsdstatustextdialog.ui
    )
    set(dsddemod_HEADERS
        ${dsddemod_HEADERS}
        dsddemodgui.h
        dsdstatustextdialog.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demoddsd)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demoddsdsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${dsddemod_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME DSDDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${dsddemod_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(LIBDSDCC_EXTERNAL)
    add_dependencies(${TARGET_NAME} dsdcc mbelib)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    ${CODEC2_LIBRARIES}
)

if(CODEC2_EXTERNAL)
    add_dependencies(${TARGET_NAME} codec2)
endif()

if(MBEVOCODER_FOUND)
    if(MBEVOCODER_INCLUDE_DIRS)
        target_include_directories(${TARGET_NAME} BEFORE PRIVATE ${MBEVOCODER_INCLUDE_DIRS})
    endif()
    target_compile_definitions(${TARGET_NAME} PRIVATE DSDDEMOD_HAS_MBELIB MBE_STATIC)
    target_link_libraries(${TARGET_NAME} PRIVATE ${MBEVOCODER_LIBRARY})
    if(MBEVOCODER_EXTERNAL)
        add_dependencies(${TARGET_NAME} mbevocoder)
    endif()
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
