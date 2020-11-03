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

if(SOAPYSDR_EXTERNAL AND NOT LINUX)
    add_dependencies(soapysdrdevice soapysdr)
endif()

install(TARGETS soapysdrdevice DESTINATION ${INSTALL_LIB_DIR})
