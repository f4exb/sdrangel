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
    ${LIBUSB_INCLUDE_DIR}
    ${LIBPERSEUS_INCLUDE_DIR}
)

add_library(perseusdevice SHARED
    ${perseusdevice_SOURCES}
)

if(LIBPERSEUS_EXTERNAL)
    add_dependencies(perseusdevice perseus)
endif()

set_target_properties(perseusdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(perseusdevice
    ${LIBUSB_LIBRARIES}
    ${LIBPERSEUS_LIBRARIES}
    sdrbase
)

install(TARGETS perseusdevice DESTINATION ${INSTALL_LIB_DIR})
