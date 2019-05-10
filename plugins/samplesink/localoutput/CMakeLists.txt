project(localoutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(localoutput_SOURCES
	localoutputgui.cpp
	localoutput.cpp
	localoutputplugin.cpp
	localoutputsettings.cpp
)

set(localoutput_HEADERS
	localoutputgui.h
	localoutput.h
	localoutputplugin.h
	localoutputsettings.h
)

set(localoutput_FORMS
	localoutputgui.ui
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(localoutput_FORMS_HEADERS ${localoutput_FORMS})

add_library(outputlocal SHARED
	${localoutput_SOURCES}
	${localoutput_HEADERS_MOC}
	${localoutput_FORMS_HEADERS}
)

target_link_libraries(outputlocal
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)

target_link_libraries(outputlocal Qt5::Core Qt5::Widgets)

install(TARGETS outputlocal DESTINATION lib/plugins/samplesink)
