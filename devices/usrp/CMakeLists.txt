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
    ${Boost_INCLUDE_DIRS}
)

add_library(usrpdevice SHARED
    ${usrpdevice_SOURCES}
)

set_target_properties(usrpdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

if (NOT WIN32)
    link_directories(${Boost_LIBRARY_DIRS})
endif()

target_link_libraries(usrpdevice
    ${UHD_LIBRARIES}
    Boost::disable_autolinking
    sdrbase
)

if(UHD_EXTERNAL)
    add_dependencies(usrpdevice uhd)
endif()

install(TARGETS usrpdevice DESTINATION ${INSTALL_LIB_DIR})
