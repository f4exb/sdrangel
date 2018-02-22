project(qrtplib)

set(qrtplib_SOURCES
	rtptimeutilities.cpp
	rtprandomurandom.cpp
	rtprandomrand48.cpp
	rtprandom.cpp
	rtperrors.cpp
)

set(qrtplib_HEADERS
	rtptimeutilities.h
	rtprandom.h
	rtprandomurandom.h
	rtprandomrand48.h
	rtprandom.h
	rtpinternalutils.h
	rtperrors.h
	rtpdefines.h
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
