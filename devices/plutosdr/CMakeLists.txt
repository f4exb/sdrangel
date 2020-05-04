project(plutosdrdevice)

set(plutosdrdevice_SOURCES
    deviceplutosdr.cpp
    deviceplutosdrbox.cpp
    deviceplutosdrscan.cpp
    deviceplutosdrparams.cpp
    deviceplutosdrshared.cpp
)

set(plutosdrdevice_HEADERS
    deviceplutosdr.h
    deviceplutsdrobox.h
    deviceplutosdrscan.h
    deviceplutosdrparams.h
    deviceplutosdrshared.h
)

include_directories(
    ${LIBIIO_INCLUDE_DIR}
    ${ICONV_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

add_library(plutosdrdevice SHARED
    ${plutosdrdevice_SOURCES}
)

set_target_properties(plutosdrdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(plutosdrdevice
    sdrbase
    ${LIBIIO_LIBRARIES}
    ${ICONV_LIBRARY}
)

if(LIBIIO_EXTERNAL)
    add_dependencies(plutosdrdevice libiio)
endif()

install(TARGETS plutosdrdevice DESTINATION ${INSTALL_LIB_DIR})
