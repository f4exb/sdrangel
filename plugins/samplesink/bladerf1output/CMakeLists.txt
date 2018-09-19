project(bladerf1output)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(bladerf1output_SOURCES
	bladerf1outputgui.cpp
	bladerf1output.cpp
	bladerf1outputplugin.cpp
	bladerf1outputsettings.cpp
	bladerf1outputthread.cpp
)

set(bladerf1output_HEADERS
	bladerf1outputgui.h
	bladerf1output.h
	bladerf1soutputplugin.h
	bladerf1outputsettings.h
	bladerf1outputthread.h
)

set(bladerf1output_FORMS
	bladerf1outputgui.ui
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

#qt4_wrap_cpp(bladerf1output_HEADERS_MOC ${bladerf1output_HEADERS})
qt5_wrap_ui(bladerf1output_FORMS_HEADERS ${bladerf1output_FORMS})

add_library(outputbladerf1 SHARED
	${bladerf1output_SOURCES}
	${bladerf1output_HEADERS_MOC}
	${bladerf1output_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(outputbladerf1
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    sdrgui
    swagger
    bladerf1device
)
else (BUILD_DEBIAN)
target_link_libraries(outputbladerf1
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	sdrgui
	swagger
	bladerf1device
)
endif (BUILD_DEBIAN)

target_link_libraries(outputbladerf1 Qt5::Core Qt5::Widgets)

install(TARGETS outputbladerf1 DESTINATION lib/plugins/samplesink)
