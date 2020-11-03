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
    ${LIBBLADERF_INCLUDE_DIRS}
)

add_library(bladerf1device SHARED
    ${bladerf1device_SOURCES}
)

if(LIBBLADERF_EXTERNAL)
    add_dependencies(bladerf1device bladerf)
endif()

set_target_properties(bladerf1device
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(bladerf1device
    ${LIBBLADERF_LIBRARIES}
    sdrbase
)

install(TARGETS bladerf1device DESTINATION ${INSTALL_LIB_DIR})
