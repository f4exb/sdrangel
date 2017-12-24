project(filesource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(filesource_SOURCES
	filesourcegui.cpp
	filesourceinput.cpp
	filesourceplugin.cpp
	filesourcethread.cpp
	filesourcesettings.cpp
)

set(filesource_HEADERS
	filesourcegui.h
	filesourceinput.h
	filesourceplugin.h
	filesourcethread.h
	filesourcesettings.h
)

set(filesource_FORMS
	filesourcegui.ui
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

#qt4_wrap_cpp(filesource_HEADERS_MOC ${filesource_HEADERS})
qt5_wrap_ui(filesource_FORMS_HEADERS ${filesource_FORMS})

add_library(inputfilesource SHARED
	${filesource_SOURCES}
	${filesource_HEADERS_MOC}
	${filesource_FORMS_HEADERS}
)

target_link_libraries(inputfilesource
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	swagger
)

qt5_use_modules(inputfilesource Core Widgets)

install(TARGETS inputfilesource DESTINATION lib/plugins/samplesource)
