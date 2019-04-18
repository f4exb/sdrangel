project(fcdhid)

set(fcdhid_SOURCES
    	../apple/apple_compat.c
	hid-libusb.c
	fcdhid.c
)

set(fcdhid_HEADERS
    	../apple/apple_compat.h
	fcdhid.h
	hid-libusb.h
	hidapi.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
        ${LIBUSB_INCLUDE_DIR}
)

#add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(fcdhid SHARED
	${fcdhid_SOURCES}
)

target_link_libraries(fcdhid
	${LIBUSB_LIBRARIES}
        ${ICONV_LIBRARY}
)

install(TARGETS fcdhid DESTINATION lib)
