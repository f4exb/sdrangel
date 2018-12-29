project(xtrxdevice)

set(xtrxdevice_SOURCES
    devicextrxparam.cpp
    devicextrxshared.cpp
)

set(xtrxdevice_HEADERS
    devicextrxparam.h
    devicextrxshared.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBXTRX_INCLUDE_DIRS}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(xtrxdevice SHARED
	${xtrxdevice_SOURCES}
)

target_link_libraries(xtrxdevice
    ${LIBXTRX_LIBRARIES}
    sdrbase
)

install(TARGETS xtrxdevice DESTINATION lib)
