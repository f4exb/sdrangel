project(fobos)

option(FOBOS_DEBUG_FILE_LOG "Write Fobos SDR diagnostic log file" OFF)

# Fobos SDR runtime packages. On Windows official builds these are expected
# to come from external/windows/fobos-sdr and external/windows/fobos-regular
# in the SDRangel Windows dependency repo.
#
# On Linux the plugin is intended to runtime-load libfobos_sdr.so / libfobos.so.
# CMake still verifies headers and libraries early so developer builds fail with
# a clear diagnostic instead of a hidden runtime loader error.
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FOBOS_SDR_PC QUIET libfobos_sdr)
        pkg_check_modules(FOBOS_REGULAR_PC QUIET libfobos)
    endif()

    set(FOBOS_SDR_INCLUDE_DIR "${FOBOS_SDR_INCLUDE_DIR}" CACHE PATH "Fobos SDR Agile include directory")
    set(FOBOS_SDR_LIBRARY "${FOBOS_SDR_LIBRARY}" CACHE FILEPATH "Fobos SDR Agile runtime library")
    set(FOBOS_REGULAR_INCLUDE_DIR "${FOBOS_REGULAR_INCLUDE_DIR}" CACHE PATH "Fobos SDR regular include directory")
    set(FOBOS_REGULAR_LIBRARY "${FOBOS_REGULAR_LIBRARY}" CACHE FILEPATH "Fobos SDR regular runtime library")

    if(NOT FOBOS_SDR_INCLUDE_DIR AND DEFINED ENV{FOBOS_SDR_DIR})
        set(FOBOS_SDR_INCLUDE_DIR "$ENV{FOBOS_SDR_DIR}/include" CACHE PATH "Fobos SDR Agile include directory" FORCE)
    endif()
    if(NOT FOBOS_SDR_LIBRARY AND DEFINED ENV{FOBOS_SDR_DIR})
        set(FOBOS_SDR_LIBRARY "$ENV{FOBOS_SDR_DIR}/lib/libfobos_sdr.so" CACHE FILEPATH "Fobos SDR Agile runtime library" FORCE)
    endif()

    if(NOT FOBOS_REGULAR_INCLUDE_DIR AND DEFINED ENV{FOBOS_DIR})
        set(FOBOS_REGULAR_INCLUDE_DIR "$ENV{FOBOS_DIR}/include" CACHE PATH "Fobos SDR regular include directory" FORCE)
    endif()
    if(NOT FOBOS_REGULAR_LIBRARY AND DEFINED ENV{FOBOS_DIR})
        set(FOBOS_REGULAR_LIBRARY "$ENV{FOBOS_DIR}/lib/libfobos.so" CACHE FILEPATH "Fobos SDR regular runtime library" FORCE)
    endif()

    if(NOT FOBOS_SDR_INCLUDE_DIR AND FOBOS_SDR_PC_INCLUDE_DIRS)
        list(GET FOBOS_SDR_PC_INCLUDE_DIRS 0 FOBOS_SDR_INCLUDE_DIR)
        set(FOBOS_SDR_INCLUDE_DIR "${FOBOS_SDR_INCLUDE_DIR}" CACHE PATH "Fobos SDR Agile include directory" FORCE)
    endif()
    if(NOT FOBOS_REGULAR_INCLUDE_DIR AND FOBOS_REGULAR_PC_INCLUDE_DIRS)
        list(GET FOBOS_REGULAR_PC_INCLUDE_DIRS 0 FOBOS_REGULAR_INCLUDE_DIR)
        set(FOBOS_REGULAR_INCLUDE_DIR "${FOBOS_REGULAR_INCLUDE_DIR}" CACHE PATH "Fobos SDR regular include directory" FORCE)
    endif()

    if(NOT FOBOS_SDR_LIBRARY AND FOBOS_SDR_PC_LIBRARY_DIRS)
        list(GET FOBOS_SDR_PC_LIBRARY_DIRS 0 FOBOS_SDR_LIBRARY_DIR)
        set(FOBOS_SDR_LIBRARY "${FOBOS_SDR_LIBRARY_DIR}/libfobos_sdr.so" CACHE FILEPATH "Fobos SDR Agile runtime library" FORCE)
    endif()
    if(NOT FOBOS_REGULAR_LIBRARY AND FOBOS_REGULAR_PC_LIBRARY_DIRS)
        list(GET FOBOS_REGULAR_PC_LIBRARY_DIRS 0 FOBOS_REGULAR_LIBRARY_DIR)
        set(FOBOS_REGULAR_LIBRARY "${FOBOS_REGULAR_LIBRARY_DIR}/libfobos.so" CACHE FILEPATH "Fobos SDR regular runtime library" FORCE)
    endif()

    if(NOT EXISTS "${FOBOS_SDR_INCLUDE_DIR}/fobos_sdr.h")
        message(FATAL_ERROR "Fobos SDR Agile header not found: ${FOBOS_SDR_INCLUDE_DIR}/fobos_sdr.h")
    endif()
    if(NOT EXISTS "${FOBOS_SDR_LIBRARY}")
        message(FATAL_ERROR "Fobos SDR Agile runtime library not found: ${FOBOS_SDR_LIBRARY}")
    endif()
    if(NOT EXISTS "${FOBOS_REGULAR_INCLUDE_DIR}/fobos.h")
        message(FATAL_ERROR "Fobos SDR regular header not found: ${FOBOS_REGULAR_INCLUDE_DIR}/fobos.h")
    endif()
    if(NOT EXISTS "${FOBOS_REGULAR_LIBRARY}")
        message(FATAL_ERROR "Fobos SDR regular runtime library not found: ${FOBOS_REGULAR_LIBRARY}")
    endif()
endif()

if(WIN32)
    set(FOBOS_SDR_INCLUDE_DIR "${FOBOS_SDR_INCLUDE_DIR}" CACHE PATH "Fobos SDR Agile include directory")
    set(FOBOS_SDR_LIBRARY "${FOBOS_SDR_LIBRARY}" CACHE FILEPATH "Fobos SDR Agile import library")
    set(FOBOS_SDR_DLL_DIR "${FOBOS_SDR_DLL_DIR}" CACHE PATH "Fobos SDR Agile runtime DLL directory")
    set(FOBOS_REGULAR_INCLUDE_DIR "${FOBOS_REGULAR_INCLUDE_DIR}" CACHE PATH "Fobos SDR regular include directory")
    set(FOBOS_REGULAR_LIBRARY "${FOBOS_REGULAR_LIBRARY}" CACHE FILEPATH "Fobos SDR regular import library")
    set(FOBOS_REGULAR_DLL_DIR "${FOBOS_REGULAR_DLL_DIR}" CACHE PATH "Fobos SDR regular runtime DLL directory")

    if(NOT EXISTS "${FOBOS_SDR_INCLUDE_DIR}/fobos_sdr.h")
        message(FATAL_ERROR "Fobos SDR Agile header not found: ${FOBOS_SDR_INCLUDE_DIR}/fobos_sdr.h")
    endif()
    if(NOT EXISTS "${FOBOS_SDR_DLL_DIR}/fobos_sdr.dll")
        message(FATAL_ERROR "Fobos SDR Agile runtime DLL not found: ${FOBOS_SDR_DLL_DIR}/fobos_sdr.dll")
    endif()
    if(NOT EXISTS "${FOBOS_REGULAR_INCLUDE_DIR}/fobos.h")
        message(FATAL_ERROR "Fobos SDR regular header not found: ${FOBOS_REGULAR_INCLUDE_DIR}/fobos.h")
    endif()
    if(NOT EXISTS "${FOBOS_REGULAR_DLL_DIR}/fobos.dll")
        message(FATAL_ERROR "Fobos SDR regular runtime DLL not found: ${FOBOS_REGULAR_DLL_DIR}/fobos.dll")
    endif()
endif()

set(FOBOS_SOURCES
    fobosinput.cpp
    fobosplugin.cpp
    fobosworker.cpp
    fobossettings.cpp
    foboswebapiadapter.cpp
)

set(FOBOS_HEADERS
    fobosinput.h
    fobosplugin.h
    fobosworker.h
    fobossettings.h
    foboswebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(FOBOS_SOURCES
        ${FOBOS_SOURCES}
        fobosgui.cpp
        fobosgui.ui
    )
    set(FOBOS_HEADERS
        ${FOBOS_HEADERS}
        fobosgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputFOBOS)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputFOBOSsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${FOBOS_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME FOBOSPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${FOBOS_SOURCES} ${FOBOS_HEADERS})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(WIN32 OR UNIX)
    target_include_directories(${TARGET_NAME} PRIVATE ${FOBOS_SDR_INCLUDE_DIR} ${FOBOS_REGULAR_INCLUDE_DIR})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
)

if(FOBOS_DEBUG_FILE_LOG)
    target_compile_definitions(${TARGET_NAME} PRIVATE FOBOS_DEBUG_FILE_LOG)
endif()

if(WIN32)
    # Make developer builds runnable without manually copying Fobos runtime files.
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FOBOS_SDR_DLL_DIR}/fobos_sdr.dll"
            "${SDRANGEL_BINARY_BIN_DIR}/fobos_sdr.dll"
        COMMENT "Copying Fobos SDR Agile runtime DLL"
    )
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FOBOS_REGULAR_DLL_DIR}/fobos.dll"
            "${SDRANGEL_BINARY_BIN_DIR}/fobos.dll"
        COMMENT "Copying Fobos SDR regular runtime DLL"
    )


    install(FILES "${FOBOS_SDR_DLL_DIR}/fobos_sdr.dll" DESTINATION ${INSTALL_BIN_DIR})
    install(FILES "${FOBOS_REGULAR_DLL_DIR}/fobos.dll" DESTINATION ${INSTALL_BIN_DIR})
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if(WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER})
endif()
