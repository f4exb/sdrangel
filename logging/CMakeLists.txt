project(logging)

set(logging_SOURCES
   dualfilelogger.cpp
   loggerwithfile.cpp
   filelogger.cpp
   logger.cpp
   logmessage.cpp
)

set(httpserver_HEADERS
   dualfilelogger.h
   loggerwithfile.h
   filelogger.h
   logger.h
   logmessage.h
   logglobal.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(logging SHARED
	${logging_SOURCES}
	${logging_HEADERS_MOC}
)

target_link_libraries(logging
	${QT_LIBRARIES}
)

qt5_use_modules(logging Core Network)

install(TARGETS logging DESTINATION lib)
