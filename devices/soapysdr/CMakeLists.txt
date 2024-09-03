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
    ${SOAPYSDR_INCLUDE_DIRS}
)

add_library(soapysdrdevice SHARED
    ${soapysdrdevice_SOURCES}
)

set_target_properties(soapysdrdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(soapysdrdevice
    ${SOAPYSDR_LINK_LIBRARIES}
    ${SOAPYSDR_LIBRARIES}
    sdrbase
)

target_link_directories(soapysdrdevice PRIVATE
    ${SOAPYSDR_LIBRARY_DIRS}
)

if(SOAPYSDR_EXTERNAL AND NOT LINUX)
    add_dependencies(soapysdrdevice soapysdr)
endif()

install(TARGETS soapysdrdevice DESTINATION ${INSTALL_LIB_DIR})
