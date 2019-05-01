project(bladerf1device)

set(bladerf1device_SOURCES
    devicebladerf1.cpp
    devicebladerf1values.cpp
    devicebladerf1shared.cpp
)

set(bladerf1device_HEADERS
    devicebladerf1.h
    devicebladerf1values.h
    devicebladerf1param.h
    devicebladerf1shared.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBBLADERF_INCLUDE_DIR}
)

add_library(bladerf1device SHARED
    ${bladerf1device_SOURCES}
)

set_target_properties(bladerf1device
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(bladerf1device
    ${LIBBLADERF_LIBRARIES}
    sdrbase
)

install(TARGETS bladerf1device DESTINATION ${INSTALL_LIB_DIR})
