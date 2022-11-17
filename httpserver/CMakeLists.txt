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
    ${CMAKE_SOURCE_DIR}/exports
)

add_library(httpserver SHARED
	${httpserver_SOURCES}
)

target_link_libraries(httpserver
    Qt::Core
    Qt::Network
)

install(TARGETS httpserver DESTINATION ${INSTALL_LIB_DIR})
