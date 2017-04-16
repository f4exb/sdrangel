project(limesdrinput)

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
    ${LIBBLADERFLIBSRC}/include
    ${LIBBLADERFLIBSRC}/src
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
    limesdr
    sdrbase
    limesdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputlimesdr
	${QT_LIBRARIES}
	${LIMESUITE_LIBRARY}
	sdrbase
	limesdrdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputlimesdr Core Widgets)

install(TARGETS inputlimesdr DESTINATION lib/plugins/samplesource)
