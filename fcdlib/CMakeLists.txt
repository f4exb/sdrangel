project(fcdlib)

set(fcdlib_SOURCES
	fcdtraits.cpp
)

set(fcdlib_HEADERS
	fcdtraits.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
)

#add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(fcdlib SHARED
	${fcdlib_SOURCES}
)

target_link_libraries(fcdlib)
