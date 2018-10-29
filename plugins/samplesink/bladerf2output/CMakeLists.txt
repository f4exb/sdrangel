project(bladerf2output)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(bladerf2output_SOURCES
	bladerf2outputgui.cpp
	bladerf2output.cpp
	bladerf2outputplugin.cpp
	bladerf2outputsettings.cpp
	bladerf2outputthread.cpp
)

set(bladerf2output_HEADERS
	bladerf2outputgui.h
	bladerf2output.h
	bladerf2outputplugin.h
	bladerf2outputsettings.h
	bladerf2outputthread.h
)

set(bladerf2output_FORMS
	bladerf2outputgui.ui
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

#qt4_wrap_cpp(bladerf2output_HEADERS_MOC ${bladerf2output_HEADERS})
qt5_wrap_ui(bladerf2output_FORMS_HEADERS ${bladerf2output_FORMS})

add_library(outputbladerf2 SHARED
	${bladerf2output_SOURCES}
	${bladerf2output_HEADERS_MOC}
	${bladerf2output_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(outputbladerf2
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    sdrgui
    swagger
    bladerf2device
)
else (BUILD_DEBIAN)
target_link_libraries(outputbladerf2
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	sdrgui
	swagger
	bladerf2device
)
endif (BUILD_DEBIAN)

target_link_libraries(outputbladerf2 Qt5::Core Qt5::Widgets)

install(TARGETS outputbladerf2 DESTINATION lib/plugins/samplesink)
