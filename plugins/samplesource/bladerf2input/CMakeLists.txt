project(bladerf2input)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(bladerf2input_SOURCES
	bladerf2inputgui.cpp
	bladerf2input.cpp
	bladerf2inputplugin.cpp
	bladerf2inputsettings.cpp
	bladerf2inputthread.cpp
)

set(bladerf2input_HEADERS
	bladerf2inputgui.h
	bladerf2input.h
	bladerf2inputplugin.h
	bladerf2inputsettings.h
	bladerf2inputthread.h
)

set(bladerf2input_FORMS
	bladerf2inputgui.ui
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

qt5_wrap_ui(bladerf2input_FORMS_HEADERS ${bladerf2input_FORMS})

add_library(inputbladerf2 SHARED
	${bladerf2input_SOURCES}
	${bladerf2input_HEADERS_MOC}
	${bladerf2input_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputbladerf2
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    sdrgui
    swagger
    bladerf2device
)
else (BUILD_DEBIAN)
target_link_libraries(inputbladerf2
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	sdrgui
	swagger
	bladerf2device
)
endif (BUILD_DEBIAN)

target_link_libraries(inputbladerf2 Qt5::Core Qt5::Widgets)

install(TARGETS inputbladerf2 DESTINATION lib/plugins/samplesource)
