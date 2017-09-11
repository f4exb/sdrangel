project(iio)

find_package(LibUSB)

set(iio_SOURCES
    ${LIBIIOSRC}/backend.c 
    ${LIBIIOSRC}/buffer.c 
    ${LIBIIOSRC}/channel.c 
    ${LIBIIOSRC}/context.c 
    ${LIBIIOSRC}/device.c 
    ${LIBIIOSRC}/iiod-client.c 
    ${LIBIIOSRC}/lock.c 
    ${LIBIIOSRC}/scan.c 
    ${LIBIIOSRC}/usb.c 
    ${LIBIIOSRC}/utilities.c 
    ${LIBIIOSRC}/xml.c 
)

set(iio_HEADERS
    ${LIBIIOSRC}/debug.h 
    ${LIBIIOSRC}/iio.h 
    ${LIBIIOSRC}/iiod-client.h 
    ${LIBIIOSRC}/iio-lock.h 
    ${LIBIIOSRC}/iio-private.h 
    include/iio-config.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}
    ${LIBIIOSRC}
    ./include
)

add_definitions(-DQT_SHARED)

add_library(iio SHARED
    ${libiio_SOURCES}
)

target_link_libraries(iio
    ${LIBUSB_LIBRARIES}
)

install(TARGETS iio DESTINATION lib)
