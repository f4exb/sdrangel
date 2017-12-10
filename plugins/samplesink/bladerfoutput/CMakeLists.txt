project(bladerfoutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(bladerfoutput_SOURCES
	bladerfoutputgui.cpp
	bladerfoutput.cpp
	bladerfoutputplugin.cpp
	bladerfoutputsettings.cpp
	bladerfoutputthread.cpp
)

set(bladerfoutput_HEADERS
	bladerfoutputgui.h
	bladerfoutput.h
	bladerfoutputplugin.h
	bladerfoutputsettings.h
	bladerfoutputthread.h
)

set(bladerfoutput_FORMS
	bladerfoutputgui.ui
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

#qt4_wrap_cpp(bladerfoutput_HEADERS_MOC ${bladerfoutput_HEADERS})
qt5_wrap_ui(bladerfoutput_FORMS_HEADERS ${bladerfoutput_FORMS})

add_library(outputbladerf SHARED
	${bladerfoutput_SOURCES}
	${bladerfoutput_HEADERS_MOC}
	${bladerfoutput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(outputbladerf
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    sdrgui
    swagger
    bladerfdevice
)
else (BUILD_DEBIAN)
target_link_libraries(outputbladerf
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	sdrgui
	swagger
	bladerfdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(outputbladerf Core Widgets)

install(TARGETS outputbladerf DESTINATION lib/plugins/samplesink)
