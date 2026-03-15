project (modemmeshtatstic)

set(modemmeshtastic_SOURCES
    meshtasticpacket.cpp
)

set(modemmeshtastic_HEADERS
    meshtasticpacket.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
)

add_library(modemmeshtastic
    ${modemmeshtastic_SOURCES}
)

target_link_libraries(modemmeshtastic
    Qt::Core
)

install(TARGETS modemmeshtastic DESTINATION ${INSTALL_LIB_DIR})
