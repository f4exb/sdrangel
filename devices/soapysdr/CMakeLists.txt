project(soapysdrdevice)

set(soapysdrdevice_SOURCES
	devicesoapysdr.cpp
    devicesoapysdrscan.cpp
    devicesoapysdrshared.cpp
    devicesoapysdrparams.cpp
)

set(soapysdrdevice_HEADERS
	devicesoapysdr.h
    devicesoapysdrscan.h
    devicesoapysdrshared.h
    devicesoapysdrparams.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SOAPYSDR_INCLUDE_DIR}
)

add_library(soapysdrdevice SHARED
    ${soapysdrdevice_SOURCES}
)

set_target_properties(soapysdrdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(soapysdrdevice
    ${SOAPYSDR_LIBRARY}
    sdrbase
)

install(TARGETS soapysdrdevice DESTINATION ${INSTALL_LIB_DIR})
