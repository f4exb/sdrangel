project(limesdrinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(limesdrinput_SOURCES
	limesdrinputgui.cpp
	limesdrinput.cpp
	limesdrinputplugin.cpp
	limesdrinputsettings.cpp
	limesdrinputthread.cpp
)

set(limesdrinput_HEADERS
	limesdrinputgui.h
	limesdrinput.h
	limesdrinputplugin.h
	limesdrinputsettings.h
	limesdrinputthread.h
)

set(limesdrinput_FORMS
	limesdrinputgui.ui
)

if (BUILD_DEBIAN)
include_directories(
        .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBLIMESUITESRC}/src
    ${LIBLIMESUITESRC}/src/ADF4002
    ${LIBLIMESUITESRC}/src/ConnectionRegistry
    ${LIBLIMESUITESRC}/src/FPGA_common
    ${LIBLIMESUITESRC}/src/GFIR
    ${LIBLIMESUITESRC}/src/lms7002m
    ${LIBLIMESUITESRC}/src/lms7002m_mcu
    ${LIBLIMESUITESRC}/src/Si5351C
    ${LIBLIMESUITESRC}/src/protocols
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/devices
	${LIMESUITE_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(limesdrinput_HEADERS_MOC ${limesdrinput_HEADERS})
qt5_wrap_ui(limesdrinput_FORMS_HEADERS ${limesdrinput_FORMS})

add_library(inputlimesdr SHARED
	${limesdrinput_SOURCES}
	${limesdrinput_HEADERS_MOC}
	${limesdrinput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputlimesdr
    ${QT_LIBRARIES}
    limesuite
    sdrbase
    sdrgui
    limesdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputlimesdr
	${QT_LIBRARIES}
	${LIMESUITE_LIBRARY}
	sdrbase
	sdrgui
	limesdrdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputlimesdr Core Widgets)

install(TARGETS inputlimesdr DESTINATION lib/plugins/samplesource)
