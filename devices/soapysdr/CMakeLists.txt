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
    ${SoapySDR_INCLUDE_DIRS}
)

add_library(soapysdrdevice SHARED
    ${soapysdrdevice_SOURCES}
)

set_target_properties(soapysdrdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(soapysdrdevice
    ${SoapySDR_LIBRARIES}
    sdrbase
)

install(TARGETS soapysdrdevice DESTINATION ${INSTALL_LIB_DIR})
