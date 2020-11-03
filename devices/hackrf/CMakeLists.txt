project(hackrfdevice)

set(hackrfdevice_SOURCES
    devicehackrf.cpp
    devicehackrfvalues.cpp
    devicehackrfshared.cpp
)

set(hackrfdevice_HEADERS
    devicehackrf.h
    devicehackrfparam.h
    devicehackrfvalues.h
    devicehackrfshared.h
)

include_directories(
    ${LIBHACKRF_INCLUDE_DIR}
)

add_library(hackrfdevice SHARED
    ${hackrfdevice_SOURCES}
)

set_target_properties(hackrfdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(hackrfdevice
    ${LIBHACKRF_LIBRARIES}
    sdrbase
)

if(LIBHACKRF_EXTERNAL AND NOT LINUX)
    add_dependencies(hackrfdevice hackrf)
endif()

install(TARGETS hackrfdevice DESTINATION ${INSTALL_LIB_DIR})
