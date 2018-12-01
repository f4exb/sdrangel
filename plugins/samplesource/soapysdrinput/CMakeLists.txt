project(soapysdrinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(soapysdrinput_SOURCES
	soapysdrinputgui.cpp
	soapysdrinput.cpp
	soapysdrinputplugin.cpp
	soapysdrinputsettings.cpp
	soapysdrinputthread.cpp
)

set(soapysdrinput_HEADERS
	soapysdrinputgui.h
	soapysdrinput.h
	soapysdrinputplugin.h
	soapysdrinputsettings.h
	soapysdrinputthread.h
)

set(soapysdrinput_FORMS
	soapysdrinputgui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${SOAPYSDR_SOURCE_DIR}/include
    ${SOAPYSDR_SOURCE_DIR}/src
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${SOAPYSDR_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(soapysdrinput_FORMS_HEADERS ${soapysdrinput_FORMS})

add_library(inputsoapysdr SHARED
	${soapysdrinput_SOURCES}
	${soapysdrinput_HEADERS_MOC}
	${soapysdrinput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputsoapysdr
    ${QT_LIBRARIES}
    soapysdr
    sdrbase
    sdrgui
    swagger
    soapysdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputsoapysdr
	${QT_LIBRARIES}
	${SOAPYSDR_LIBRARY}
	sdrbase
	sdrgui
	swagger
	soapysdrdevice
)
endif (BUILD_DEBIAN)

target_link_libraries(inputsoapysdr Qt5::Core Qt5::Widgets)

install(TARGETS inputsoapysdr DESTINATION lib/plugins/samplesource)
