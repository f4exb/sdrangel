project(soapysdroutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(soapysdroutput_SOURCES
	soapysdroutputgui.cpp
	soapysdroutput.cpp
	soapysdroutputplugin.cpp
#	soapysdroutputsettings.cpp
#	soapysdroutputthread.cpp
)

set(soapysdroutput_HEADERS
	soapysdroutputgui.h
	soapysdroutput.h
	soapysdroutputplugin.h
#	soapysdroutputsettings.h
#	soapysdroutputthread.h
)

set(soapysdroutput_FORMS
#	soapysdroutputgui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices
    ${SOAPYSDRSRC}/include
    ${SOAPYSDRSRC}/src
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

qt5_wrap_ui(soapysdroutput_FORMS_HEADERS ${soapysdroutput_FORMS})

add_library(outputsoapysdr SHARED
	${soapysdroutput_SOURCES}
	${soapysdroutput_HEADERS_MOC}
	${soapysdroutput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(outputsoapysdr
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    sdrgui
    swagger
    soapysdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(outputsoapysdr
	${QT_LIBRARIES}
	${SOAPYSDR_LIBRARY}
	sdrbase
	sdrgui
	swagger
	soapysdrdevice
)
endif (BUILD_DEBIAN)

target_link_libraries(outputsoapysdr Qt5::Core Qt5::Widgets)

install(TARGETS outputsoapysdr DESTINATION lib/plugins/samplesink)
