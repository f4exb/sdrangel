project(usrpdevice)


set(usrpdevice_SOURCES
    deviceusrp.cpp
    deviceusrpparam.cpp
    deviceusrpshared.cpp
)

set(usrpdevice_HEADERS
    deviceusrp.h
    deviceusrpparam.h
    deviceusrpshared.h
)

include_directories(
    ${UHD_INCLUDE_DIR}
)

add_library(usrpdevice SHARED
    ${usrpdevice_SOURCES}
)

set_target_properties(usrpdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(usrpdevice
    ${UHD_LIBRARIES}
    Boost::disable_autolinking
    sdrbase
)

if(UHD_EXTERNAL)
    add_dependencies(usrpdevice uhd)
endif()

install(TARGETS usrpdevice DESTINATION ${INSTALL_LIB_DIR})
