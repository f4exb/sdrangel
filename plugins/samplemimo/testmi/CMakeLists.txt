project(testmi)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(testmi_SOURCES
	testmigui.cpp
	testmi.cpp
	testmiplugin.cpp
	testmithread.cpp
	testmisettings.cpp
)

set(testmi_HEADERS
	testmigui.h
	testmi.h
	testmiplugin.h
	testmithread.h
	testmisettings.h
)

set(testmi_FORMS
	testmigui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(testmi_FORMS_HEADERS ${testmi_FORMS})

add_library(mimotestmi SHARED
	${testmi_SOURCES}
	${testmi_HEADERS_MOC}
	${testmi_FORMS_HEADERS}
)

target_link_libraries(mimotestmi
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	swagger
)

target_link_libraries(mimotestmi Qt5::Core Qt5::Widgets)

install(TARGETS mimotestmi DESTINATION lib/plugins/samplemimo)
