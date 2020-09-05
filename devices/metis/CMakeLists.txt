project(metisdevice)

set(metisdevice_SOURCES
    devicemetis.cpp
    devicemetisscan.cpp
)

set(metisdevice_HEADERS
    devicemetis.h
    devicemetisscan.h
)

add_library(metisdevice SHARED
    ${metisdevice_SOURCES}
)

set_target_properties(metisdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(metisdevice
    sdrbase
)

install(TARGETS metisdevice DESTINATION ${INSTALL_LIB_DIR})
