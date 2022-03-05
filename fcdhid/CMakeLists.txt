project(fcdhid)

set(fcdhid_SOURCES
    ../custom/apple/apple_compat.c
    fcdhid.c
)

set(fcdhid_HEADERS
    fcdhid.h
)

include_directories(
    ${LIBUSB_INCLUDE_DIR}
	${HIDAPI_INCLUDE_DIRS}
    ${ICONV_INCLUDE_DIR}
    ${CUSTOM_APPLE_INCLUDE}
)

add_library(fcdhid SHARED
    ${fcdhid_SOURCES}
)

target_link_libraries(fcdhid
    ${LIBUSB_LIBRARIES}
	${HIDAPI_LIBRARIES}
    ${ICONV_LIBRARY}
)

install(TARGETS fcdhid DESTINATION ${INSTALL_LIB_DIR})
