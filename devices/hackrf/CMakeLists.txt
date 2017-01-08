project(hackrfdevice)

set(hackrfdevice_SOURCES
    devicehackrf.cpp
    devicehackrfvalues.cpp
)

set(hackrfdevice_HEADERS
    devicehackrf.h
    devicehackrfparam.h
    devicehackrfvalues.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBHACKRFLIBSRC}/include
    ${LIBHACKRFLIBSRC}/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBHACKRF_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#add_definitions(${QT_DEFINITIONS})
#add_definitions(-DQT_SHARED)

add_library(hackrfdevice SHARED
    ${hackrfdevice_SOURCES}
)

if (BUILD_DEBIAN)
target_link_libraries(hackrfdevice
    hackrf
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(hackrfdevice
    ${LIBHACKRF_LIBRARIES}
    sdrbase
)
endif (BUILD_DEBIAN)

install(TARGETS hackrfdevice DESTINATION lib)
