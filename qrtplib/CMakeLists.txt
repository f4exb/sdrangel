project(qrtplib)

set(qrtplib_SOURCES
	rtperrors.cpp
	rtppacket.cpp
	rtprandom.cpp
	rtprandomrand48.cpp
	rtprandomurandom.cpp
	rtptimeutilities.cpp
)

set(qrtplib_HEADERS
	rtperrors.h
	rtpdefines.h
	rtpinternalutils.h
	rtppacket.h
	rtprandom.h
	rtprandomrand48.h
	rtprandomurandom.h
	rtprawpacket.h
	rtpstructs.h
	rtptimeutilities.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(qrtplib SHARED
	${qrtplib_SOURCES}
	${qrtplib_HEADERS_MOC}
)

target_link_libraries(qrtplib
	${QT_LIBRARIES}
)

qt5_use_modules(qrtplib Core Network)

install(TARGETS qrtplib DESTINATION lib)
