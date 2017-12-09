project(bladerfinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(bladerfinput_SOURCES
	bladerfinputgui.cpp
	bladerfinput.cpp
	bladerfinputplugin.cpp
	bladerfinputsettings.cpp
	bladerfinputthread.cpp
)

set(bladerfinput_HEADERS
	bladerfinputgui.h
	bladerfinput.h
	bladerfinputplugin.h
	bladerfinputsettings.h
	bladerfinputthread.h
)

set(bladerfinput_FORMS
	bladerfinputgui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBBLADERFLIBSRC}/include
    ${LIBBLADERFLIBSRC}/src
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
	${CMAKE_SOURCE_DIR}/devices
	${LIBBLADERF_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(bladerfinput_HEADERS_MOC ${bladerfinput_HEADERS})
qt5_wrap_ui(bladerfinput_FORMS_HEADERS ${bladerfinput_FORMS})

add_library(inputbladerf SHARED
	${bladerfinput_SOURCES}
	${bladerfinput_HEADERS_MOC}
	${bladerfinput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputbladerf
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    sdrgui
    swagger
    bladerfdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputbladerf
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	sdrgui
	swagger
	bladerfdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputbladerf Core Widgets)

install(TARGETS inputbladerf DESTINATION lib/plugins/samplesource)
