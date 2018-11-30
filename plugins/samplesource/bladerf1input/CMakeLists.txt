project(bladerf1input)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(bladerf1input_SOURCES
	bladerf1inputgui.cpp
	bladerf1input.cpp
	bladerf1inputplugin.cpp
	bladerf1inputsettings.cpp
	bladerf1inputthread.cpp
)

set(bladerf1input_HEADERS
	bladerf1inputgui.h
	bladerf1input.h
	bladerf1inputplugin.h
	bladerf1inputsettings.h
	bladerf1inputthread.h
)

set(bladerf1input_FORMS
	bladerf1inputgui.ui
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

#qt4_wrap_cpp(bladerf1input_HEADERS_MOC ${bladerf1input_HEADERS})
qt5_wrap_ui(bladerf1input_FORMS_HEADERS ${bladerf1input_FORMS})

add_library(inputbladerf1 SHARED
	${bladerf1input_SOURCES}
	${bladerf1input_HEADERS_MOC}
	${bladerf1input_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputbladerf1
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    sdrgui
    swagger
    bladerf1device
)
else (BUILD_DEBIAN)
target_link_libraries(inputbladerf1
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	sdrgui
	swagger
	bladerf1device
)
endif (BUILD_DEBIAN)

target_link_libraries(inputbladerf1 Qt5::Core Qt5::Widgets)

install(TARGETS inputbladerf1 DESTINATION lib/plugins/samplesource)
