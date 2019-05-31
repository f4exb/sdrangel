project(fcdlib)

set(fcdlib_SOURCES
	fcdtraits.cpp
	fcdproplusconst.cpp
	fcdproconst.cpp
)

set(fcdlib_HEADERS
	fcdtraits.h
	fcdproplusconst.h
	fcdproconst.h
)

include_directories(
)

add_library(fcdlib SHARED
	${fcdlib_SOURCES}
)

target_link_libraries(fcdlib)

install(TARGETS fcdlib DESTINATION ${INSTALL_LIB_DIR})
