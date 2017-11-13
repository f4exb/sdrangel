project(httpserver)

set(httpserver_SOURCES
   httpglobal.cpp
   httplistener.cpp
   httpconnectionhandler.cpp
   httpconnectionhandlerpool.cpp
   httprequest.cpp
   httpresponse.cpp
   httpcookie.cpp
   httprequesthandler.cpp
   httpsession.cpp
   httpsessionstore.cpp
   staticfilecontroller.cpp
)

set(httpserver_HEADERS
   httpglobal.h
   httplistener.h
   httpconnectionhandler.h
   httpconnectionhandlerpool.h
   httprequest.h
   httpresponse.h
   httpcookie.h
   httprequesthandler.h
   httpsession.h
   httpsessionstore.h
   staticfilecontroller.h
   httplistenersettings.h
   httpdocrootsettings.h
   httpsessionssettings.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(httpserver SHARED
	${httpserver_SOURCES}
	${httpserver_HEADERS_MOC}
)

target_link_libraries(httpserver
	${QT_LIBRARIES}
)

qt5_use_modules(httpserver Core Network)

install(TARGETS httpserver DESTINATION lib)
