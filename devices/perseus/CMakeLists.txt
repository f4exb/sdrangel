project(perseusdevice)

set(perseusdevice_SOURCES
    deviceperseus.cpp
    deviceperseusscan.cpp
)

set(perseusdevice_HEADERS
    deviceperseus.h
    deviceperseusscan.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBPERSEUS_INCLUDE_DIR}
)

add_library(perseusdevice SHARED
    ${perseusdevice_SOURCES}
)

set_target_properties(perseusdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(perseusdevice
    ${LIBPERSEUS_LIBRARIES}
    sdrbase
)

install(TARGETS perseusdevice DESTINATION ${INSTALL_LIB_DIR})
