project(demodacars)

set(demodacars_SOURCES
    acarsdemod.cpp
    acarsdemodsettings.cpp
    acarsdemodbaseband.cpp
    acarsdemodsink.cpp
    acarsdemodplugin.cpp
    acarsdemodwebapiadapter.cpp
    acarsdemodworker.cpp
    acarsdemoddata.qrc
)

set(demodacars_HEADERS
    acarsdemod.h
    acarsdemodsettings.h
    acarsdemodbaseband.h
    acarsdemodsink.h
    acarsdemodplugin.h
    acarsdemodwebapiadapter.h
    acarsdemodworker.h
    acarsoqpsk.h
    acarsvdl2.h
    acarshfdl.h
    acarsaero.h
    acarsiridium.h
    acarsiridiumframe.h
    acarsiridiumfft.h
    acarsiridiumchannelizer.h
    acarsiridiumscanner.h
    vdl2atn.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBACARS_INCLUDE_DIR}
)

# The ATN decoder for VDL-2 information frames: X.25/CLNP/COTP in vdl2atn.cpp and the
# ICAO ASN.1 applications (CM, CPDLC, ADS-C v2) vendored from dumpvdl2 (GPL-3.0) in
# atn/. Built as a static library so the offline test harness in test/ can link the
# same decoder. Used by the worker, so built in both GUI and server modes.
file(GLOB demodacars_ATN_ASN1_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/atn/asn1/*.c)

add_library(demodacarsatn STATIC
    vdl2atn.cpp
    vdl2atn.h
    atn/icao.c
    atn/xid.c
    atn/tlv.c
    atn/asn1-util.c
    atn/asn1-format-icao-text.c
    atn/asn1-format-icao-json.c
    atn/dumpvdl2shim.c
    ${demodacars_ATN_ASN1_SOURCES}
)
target_include_directories(demodacarsatn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/atn ${LIBACARS_INCLUDE_DIR})
target_link_libraries(demodacarsatn PRIVATE Qt::Core ${LIBACARS_LIBRARIES})
if(DEFINED LIBACARS_DEPENDS)
    add_dependencies(demodacarsatn ${LIBACARS_DEPENDS})
endif()
# The vendored dumpvdl2 sources use C11 designated initializers
set_property(TARGET demodacarsatn PROPERTY C_STANDARD 11)
set_property(TARGET demodacarsatn PROPERTY POSITION_INDEPENDENT_CODE ON)

if(NOT SERVER_MODE)
    set(demodacars_SOURCES
        ${demodacars_SOURCES}
        acarsdemodgui.cpp
        acarsmessagemodel.cpp
        acarsdemodgui.ui
        acarsdemodicons.qrc
    )
    set(demodacars_HEADERS
        ${demodacars_HEADERS}
        acarsdemodgui.h
        acarsmessagemodel.h
    )

    set(TARGET_NAME demodacars)
    set(TARGET_LIB "Qt::Widgets" Qt::Quick Qt::QuickWidgets Qt::Positioning Qt::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodacarssrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} SHARED ${demodacars_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME AcarsDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demodacars_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(DEFINED LIBACARS_DEPENDS)
    add_dependencies(${TARGET_NAME} ${LIBACARS_DEPENDS})
endif()

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    ${LIBACARS_LIBRARIES}
)

target_link_libraries(${TARGET_NAME} demodacarsatn)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
