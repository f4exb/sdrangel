project(spectrandevice)

set(spectrandevice_SOURCES
    devicespectran.cpp
    aaroniartsasdkhelper.cpp
)

set(spectrandevice_HEADERS
    devicespectran.h
    aaroniartsasdkhelper.h
)

include_directories(
    ${RTSAAPI_INCLUDE_DIR}
)

add_library(spectrandevice SHARED
    ${spectrandevice_SOURCES}
)

set_target_properties(spectrandevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(spectrandevice
    ${RTSAAPI_LIBRARY}
    sdrbase
)

install(TARGETS spectrandevice DESTINATION ${INSTALL_LIB_DIR})
