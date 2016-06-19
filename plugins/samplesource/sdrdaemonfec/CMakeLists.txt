project(sdrdaemonfec)

find_package(LZ4)
find_package(LibNANOMSG)

set(sdrdaemonfec_SOURCES
    sdrdaemonfecbuffer.cpp
	sdrdaemonfecgui.cpp
	sdrdaemonfecinput.cpp
	sdrdaemonfecplugin.cpp
	sdrdaemonfecudphandler.cpp
)

set(sdrdaemonfec_HEADERS
    sdrdaemonfecbuffer.h
	sdrdaemonfecgui.h
	sdrdaemonfecinput.h
	sdrdaemonfecplugin.h
	sdrdaemonfecudphandler.h
)

set(sdrdaemonfec_FORMS
	sdrdaemonfecgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(sdrdaemonfec_HEADERS_MOC ${sdrdaemonfec_HEADERS})
qt5_wrap_ui(sdrdaemonfec_FORMS_HEADERS ${sdrdaemonfec_FORMS})

add_library(inputsdrdaemonfec SHARED
	${sdrdaemonfec_SOURCES}
	${sdrdaemonfec_HEADERS_MOC}
	${sdrdaemonfec_FORMS_HEADERS}
)

target_include_directories(inputsdrdaemonfec PUBLIC
    ${LZ4_INCLUDE_DIRS}
    ${LIBNANOMSG_INCLUDE_DIR}
)

target_link_libraries(inputsdrdaemonfec
	${QT_LIBRARIES}
	${LZ4_LIBRARIES}
	${LIBNANOMSG_LIBRARIES}
	sdrbase
)

qt5_use_modules(inputsdrdaemonfec Core Widgets OpenGL Multimedia)

install(TARGETS inputsdrdaemonfec DESTINATION lib/plugins/samplesource)
