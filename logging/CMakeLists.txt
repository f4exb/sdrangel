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
    ${CMAKE_SOURCE_DIR}/exports
)

add_library(logging SHARED
    ${logging_SOURCES}
)

target_link_libraries(logging
    Qt::Core
    Qt::Network
)

install(TARGETS logging DESTINATION ${INSTALL_LIB_DIR})
