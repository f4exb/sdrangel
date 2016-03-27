project(sdrdaemon)

find_package(LZ4)
find_package(LibNANOMSG)

set(sdrdaemon_SOURCES
    sdrdaemonbuffer.cpp
	sdrdaemongui.cpp
	sdrdaemoninput.cpp
	sdrdaemonplugin.cpp
	sdrdaemonudphandler.cpp
)

set(sdrdaemon_HEADERS
    sdrdaemonbuffer.h
	sdrdaemongui.h
	sdrdaemoninput.h
	sdrdaemonplugin.h
	sdrdaemonudphandler.h
)

set(sdrdaemon_FORMS
	sdrdaemongui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(sdrdaemon_HEADERS_MOC ${sdrdaemon_HEADERS})
qt5_wrap_ui(sdrdaemon_FORMS_HEADERS ${sdrdaemon_FORMS})

add_library(inputsdrdaemon SHARED
	${sdrdaemon_SOURCES}
	${sdrdaemon_HEADERS_MOC}
	${sdrdaemon_FORMS_HEADERS}
)

target_include_directories(inputsdrdaemon PUBLIC
    ${LZ4_INCLUDE_DIRS}
    ${LIBNANOMSG_INCLUDE_DIR}
)

target_link_libraries(inputsdrdaemon
	${QT_LIBRARIES}
	${LZ4_LIBRARIES}
	${LIBNANOMSG_LIBRARIES}
	sdrbase
)

qt5_use_modules(inputsdrdaemon Core Widgets OpenGL Multimedia)

install(TARGETS inputsdrdaemon DESTINATION lib/plugins/samplesource)
