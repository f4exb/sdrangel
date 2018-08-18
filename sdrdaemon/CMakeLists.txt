project (sdrdaemon)

set(sdrdaemon_SOURCES
    sdrdaemonmain.cpp
    sdrdaemonpreferences.cpp
    sdrdaemonsettings.cpp  
    sdrdaemonparser.cpp
    webapi/webapiadapterdaemon.cpp
    webapi/webapirequestmapper.cpp
    webapi/webapiserver.cpp
)

set(sdrdaemon_HEADERS
    sdrdaemonmain.h
    sdrdaemonpreferences.h
    sdrdaemonsettings.h  
    sdrdaemonparser.h
    webapi/webapiadapterdaemon.h
    webapi/webapirequestmapper.h
    webapi/webapiserver.h
)

set(sdrdaemon_SOURCES
    ${sdrdaemon_SOURCES}
    ${sdrdaemon_HEADERS}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(sdrdaemon SHARED
    ${sdrdaemon_SOURCES}
    ${sdrdaemon_HEADERS_MOC}
)

include_directories(
    .
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/logging
    ${CMAKE_SOURCE_DIR}/httpserver
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(sdrdaemon
    ${QT_LIBRARIES}
    sdrbase
    logging
)

target_compile_features(sdrdaemon PRIVATE cxx_generalized_initializers) # cmake >= 3.1.0

target_link_libraries(sdrdaemon Qt5::Core Qt5::Multimedia)

install(TARGETS sdrdaemon DESTINATION lib)

