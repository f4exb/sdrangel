project(testsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(testsource_SOURCES
	testsourcegui.cpp
	testsourceinput.cpp
	testsourceplugin.cpp
	testsourcethread.cpp
	testsourcesettings.cpp
)

set(testsource_HEADERS
	testsourcegui.h
	testsourceinput.h
	testsourceplugin.h
	testsourcethread.h
	testsourcesettings.h
)

set(testsource_FORMS
	testsourcegui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(testsource_HEADERS_MOC ${testsource_HEADERS})
qt5_wrap_ui(testsource_FORMS_HEADERS ${testsource_FORMS})

add_library(inputtestsource SHARED
	${testsource_SOURCES}
	${testsource_HEADERS_MOC}
	${testsource_FORMS_HEADERS}
)

target_link_libraries(inputtestsource
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	swagger
)

qt5_use_modules(inputtestsource Core Widgets)

install(TARGETS inputtestsource DESTINATION lib/plugins/samplesource)
