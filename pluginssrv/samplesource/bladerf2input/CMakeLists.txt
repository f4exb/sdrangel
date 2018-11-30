project(bladerf2input)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/bladerf2input")

set(bladerf2input_SOURCES
	${PLUGIN_PREFIX}/bladerf2input.cpp
	${PLUGIN_PREFIX}/bladerf2inputplugin.cpp
	${PLUGIN_PREFIX}/bladerf2inputsettings.cpp
	${PLUGIN_PREFIX}/bladerf2inputthread.cpp
)

set(bladerf2input_HEADERS
	${PLUGIN_PREFIX}/bladerf2input.h
	${PLUGIN_PREFIX}/bladerf2inputplugin.h
	${PLUGIN_PREFIX}/bladerf2inputsettings.h
	${PLUGIN_PREFIX}/bladerf2inputthread.h
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

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputbladerf2srv SHARED
	${bladerf2input_SOURCES}
	${bladerf2input_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputbladerf2srv
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    swagger
    bladerf2device
)
else (BUILD_DEBIAN)
target_link_libraries(inputbladerf2srv
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	swagger
	bladerf2device
)
endif (BUILD_DEBIAN)

target_link_libraries(inputbladerf2srv Qt5::Core)

install(TARGETS inputbladerf2srv DESTINATION lib/pluginssrv/samplesource)
