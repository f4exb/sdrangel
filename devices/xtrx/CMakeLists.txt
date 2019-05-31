project(xtrxdevice)

set(xtrxdevice_SOURCES
    devicextrx.cpp
    devicextrxparam.cpp
    devicextrxshared.cpp
)

set(xtrxdevice_HEADERS
    devicextrx.h
    devicextrxparam.h
    devicextrxshared.h
)

include_directories(
    ${LIBXTRX_INCLUDE_DIRS}
)

add_library(xtrxdevice SHARED
    ${xtrxdevice_SOURCES}
)

set_target_properties(xtrxdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(xtrxdevice
    ${LIBXTRX_LIBRARY}
    sdrbase
)

install(TARGETS xtrxdevice DESTINATION ${INSTALL_LIB_DIR})
