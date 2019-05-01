project(bladerf2device)

set(bladerf2device_SOURCES
    devicebladerf2.cpp
    devicebladerf2shared.cpp
)

set(bladerf2device_HEADERS
    devicebladerf2.h
    devicebladerf2shared.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBBLADERF_INCLUDE_DIR}
)

add_library(bladerf2device SHARED
    ${bladerf2device_SOURCES}
)

set_target_properties(bladerf2device
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(bladerf2device
    ${LIBBLADERF_LIBRARIES}
    sdrbase
)

install(TARGETS bladerf2device DESTINATION ${INSTALL_LIB_DIR})
