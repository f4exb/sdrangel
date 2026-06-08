project(fobos)

option(FOBOS_DEBUG_FILE_LOG "Write Fobos SDR diagnostic log file" OFF)

# Fobos SDR runtime packages.
#
# Windows: official builds expect the import libraries / DLLs to come from
# external/windows/fobos-sdr and external/windows/fobos-regular in the SDRangel
# Windows dependency repo (handled in the WIN32 block below).
#
# Linux: the plugin does NOT link against or #include the Fobos SDK. fobosworker.{h,cpp}
# forward-declare fobos_dev_t and define their own function-pointer typedefs, then load
# libfobos_sdr.so / libfobos.so at runtime through QLibrary. Consequently neither the
# development headers nor the runtime libraries are required at build time, and any
# combination of backends (Agile, Regular, both or none) is resolved at runtime.
# The detection below is therefore best-effort and ADVISORY ONLY: it never fails the
# build, and it is multilib-aware (lib/lib64) so it does not depend on a possibly
# incorrect libfobos_sdr.pc libdir.
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FOBOS_SDR_PC QUIET libfobos_sdr)
        pkg_check_modules(FOBOS_REGULAR_PC QUIET libfobos)
    endif()

    # Optional include dirs. Not required to build (no vendor header is #included),
    # but honoured if the user points at a custom SDK location.
    set(FOBOS_SDR_INCLUDE_DIR "${FOBOS_SDR_INCLUDE_DIR}" CACHE PATH "Optional Fobos SDR Agile include directory (not required on Linux)")
    set(FOBOS_REGULAR_INCLUDE_DIR "${FOBOS_REGULAR_INCLUDE_DIR}" CACHE PATH "Optional Fobos SDR regular include directory (not required on Linux)")

    if(NOT FOBOS_SDR_INCLUDE_DIR AND DEFINED ENV{FOBOS_SDR_DIR})
        set(FOBOS_SDR_INCLUDE_DIR "$ENV{FOBOS_SDR_DIR}/include" CACHE PATH "Optional Fobos SDR Agile include directory (not required on Linux)" FORCE)
    endif()
    if(NOT FOBOS_REGULAR_INCLUDE_DIR AND DEFINED ENV{FOBOS_DIR})
        set(FOBOS_REGULAR_INCLUDE_DIR "$ENV{FOBOS_DIR}/include" CACHE PATH "Optional Fobos SDR regular include directory (not required on Linux)" FORCE)
    endif()
    if(NOT FOBOS_SDR_INCLUDE_DIR AND FOBOS_SDR_PC_INCLUDE_DIRS)
        list(GET FOBOS_SDR_PC_INCLUDE_DIRS 0 FOBOS_SDR_INCLUDE_DIR)
    endif()
    if(NOT FOBOS_REGULAR_INCLUDE_DIR AND FOBOS_REGULAR_PC_INCLUDE_DIRS)
        list(GET FOBOS_REGULAR_PC_INCLUDE_DIRS 0 FOBOS_REGULAR_INCLUDE_DIR)
    endif()

    # Advisory runtime-library probe. find_library is multilib-aware and can search
    # common lib/lib64 suffixes, so it is not dependent on a possibly wrong .pc libdir.
    # The result is informational only and never gates the build.
    find_library(FOBOS_SDR_LIBRARY
        NAMES fobos_sdr
        HINTS "$ENV{FOBOS_SDR_DIR}" ${FOBOS_SDR_PC_PREFIX} ${FOBOS_SDR_PC_LIBRARY_DIRS}
        PATH_SUFFIXES lib lib64 lib/${CMAKE_LIBRARY_ARCHITECTURE}
    )
    find_library(FOBOS_REGULAR_LIBRARY
        NAMES fobos
        HINTS "$ENV{FOBOS_DIR}" ${FOBOS_REGULAR_PC_PREFIX} ${FOBOS_REGULAR_PC_LIBRARY_DIRS}
        PATH_SUFFIXES lib lib64 lib/${CMAKE_LIBRARY_ARCHITECTURE}
    )

    if(FOBOS_SDR_LIBRARY)
        message(STATUS "Fobos SDR: Agile runtime detected (advisory): ${FOBOS_SDR_LIBRARY}")
    else()
        message(STATUS "Fobos SDR: Agile runtime not found at configure time; it will be loaded at runtime via QLibrary if libfobos_sdr.so is installed.")
    endif()
    if(FOBOS_REGULAR_LIBRARY)
        message(STATUS "Fobos SDR: Regular runtime detected (advisory): ${FOBOS_REGULAR_LIBRARY}")
    else()
        message(STATUS "Fobos SDR: Regular runtime not found at configure time; it will be loaded at runtime via QLibrary if libfobos.so is installed.")
    endif()
    if(NOT FOBOS_SDR_LIBRARY AND NOT FOBOS_REGULAR_LIBRARY)
        message(STATUS "Fobos SDR: no runtime library detected at configure time. The plugin still builds; install libfobos_sdr.so and/or libfobos.so to use the device.")
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
