project(fcdhid)

set(fcdhid_SOURCES
    	../custom/apple/apple_compat.c
	hid-libusb.c
	fcdhid.c
)

set(fcdhid_HEADERS
    	../custom/apple/apple_compat.h
	fcdhid.h
	hid-libusb.h
	hidapi.h
)

include_directories(
        ${LIBUSB_INCLUDE_DIR}
        ${ICONV_INCLUDE_DIR}
)

add_library(fcdhid SHARED
	${fcdhid_SOURCES}
)

target_link_libraries(fcdhid
        ${LIBUSB_LIBRARIES}
        ${ICONV_LIBRARY}
)

install(TARGETS fcdhid DESTINATION ${INSTALL_LIB_DIR})
