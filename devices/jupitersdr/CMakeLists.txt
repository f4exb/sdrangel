project(jupitersdrdevice)

set(jupitersdrdevice_SOURCES
    devicejupitersdr.cpp
    devicejupitersdrbox.cpp
    devicejupitersdrscan.cpp
    devicejupitersdrparams.cpp
    devicejupitersdrshared.cpp
)

set(jupitersdrdevice_HEADERS
    devicejupitersdr.h
    devicejupitersdrobox.h
    devicejupitersdrscan.h
    devicejupitersdrparams.h
    devicejupitersdrshared.h
)

include_directories(
    ${LIBIIO_INCLUDE_DIR}
    ${ICONV_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

add_library(jupitersdrdevice SHARED
    ${jupitersdrdevice_SOURCES}
)

set_target_properties(jupitersdrdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(jupitersdrdevice
    sdrbase
    ${LIBIIO_LIBRARIES}
    ${ICONV_LIBRARY}
)

if(LIBIIO_EXTERNAL)
    add_dependencies(jupitersdrdevice libiio)
endif()

install(TARGETS jupitersdrdevice DESTINATION ${INSTALL_LIB_DIR})
