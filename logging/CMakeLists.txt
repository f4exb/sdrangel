project(logging)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

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
    ${CMAKE_SOURCE_DIR}/exports 
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(logging SHARED
	${logging_SOURCES}
	${logging_HEADERS_MOC}
)

set_target_properties(logging PROPERTIES DEFINE_SYMBOL "logging_EXPORTS")

target_link_libraries(logging
	${QT_LIBRARIES}
)

target_link_libraries(logging Qt5::Core Qt5::Network)

install(TARGETS logging DESTINATION lib)
