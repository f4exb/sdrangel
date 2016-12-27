project(bladerf)

set(bladerf_SOURCES
	bladerfinputgui.cpp
	bladerfinput.cpp
	bladerfinputplugin.cpp
	bladerfinputsettings.cpp
	bladerfinputthread.cpp
)

set(bladerf_HEADERS
	bladerfinputgui.h
	bladerfinput.h
	bladerfinputplugin.h
	bladerfinputsettings.h
	bladerfinputthread.h
)

set(bladerf_FORMS
	bladerfinputgui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBBLADERFLIBSRC}/include
    ${LIBBLADERFLIBSRC}/src
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${LIBBLADERF_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(bladerf_HEADERS_MOC ${bladerf_HEADERS})
qt5_wrap_ui(bladerf_FORMS_HEADERS ${bladerf_FORMS})

add_library(inputbladerf SHARED
	${bladerf_SOURCES}
	${bladerf_HEADERS_MOC}
	${bladerf_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputbladerf
    ${QT_LIBRARIES}
    bladerf
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(inputbladerf
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputbladerf Core Widgets)

install(TARGETS inputbladerf DESTINATION lib/plugins/samplesource)
