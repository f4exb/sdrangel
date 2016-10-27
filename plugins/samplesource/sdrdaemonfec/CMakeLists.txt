project(sdrdaemonfec)

find_package(LibNANOMSG)

EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
message( STATUS "SDRdaemonFEC: Architecture: ${ARCHITECTURE}" )

if(${ARCHITECTURE} MATCHES "x86_64|AMD64|x86")
    SET(USE_SIMD "SSSE3")
elseif(${ARCHITECTURE} MATCHES "armv7l")
    SET(USE_SIMD "NEON")
endif()

message( STATUS "SDRdaemonFEC: use SIMD: ${USE_SIMD}" )

if(USE_SIMD MATCHES SSSE3)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mssse3" )
        set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mssse3" )
        message(STATUS "SDRdaemonFEC: g++ SSSE3")
        add_definitions(-DUSE_SIMD)
    elseif(MSVC)
        set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSSE3" )
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSSE3" )
        set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
        message(STATUS "SDRdaemonFEC: MSVC SSSE3")
        add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
        add_definitions(-DUSE_SIMD)
    endif()
elseif(USE_SIMD MATCHES NEON)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpu=neon" )
        set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mfpu=neon" )
        message(STATUS "SDRdaemonFEC: g++ NEON")
        add_definitions(-DUSE_NEON)
    endif()
else()
    message(STATUS "SDRdaemonFEC: Unsupported architecture")
    return()
endif()

set(sdrdaemonfec_SOURCES
    sdrdaemonfecbuffer.cpp
    sdrdaemonfecgui.cpp
    sdrdaemonfecinput.cpp
    sdrdaemonfecplugin.cpp
    sdrdaemonfecudphandler.cpp
)

set(sdrdaemonfec_HEADERS
    sdrdaemonfecbuffer.h
    sdrdaemonfecgui.h
    sdrdaemonfecinput.h
    sdrdaemonfecplugin.h
    sdrdaemonfecudphandler.h
)

set(sdrdaemonfec_FORMS
    sdrdaemonfecgui.ui
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(sdrdaemonfec_HEADERS_MOC ${sdrdaemonfec_HEADERS})
qt5_wrap_ui(sdrdaemonfec_FORMS_HEADERS ${sdrdaemonfec_FORMS})

add_library(inputsdrdaemonfec SHARED
    ${sdrdaemonfec_SOURCES}
    ${sdrdaemonfec_HEADERS_MOC}
    ${sdrdaemonfec_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_include_directories(inputsdrdaemonfec PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBCM256CCSRC}
    ${LIBNANOMSG_INCLUDE_DIR}
)
else (BUILD_DEBIAN)
target_include_directories(inputsdrdaemonfec PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CM256CC_INCLUDE_DIR}
    ${LIBNANOMSG_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(inputsdrdaemonfec
    ${QT_LIBRARIES}
    cm256cc
    ${LIBNANOMSG_LIBRARIES}
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(inputsdrdaemonfec
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
    ${LIBNANOMSG_LIBRARIES}
    sdrbase
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputsdrdaemonfec Core Widgets)

install(TARGETS inputsdrdaemonfec DESTINATION lib/plugins/samplesource)
