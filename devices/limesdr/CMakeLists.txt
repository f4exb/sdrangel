project(limesdrdevice)

set(limesdrdevice_SOURCES
    devicelimesdr.cpp
    devicelimesdrparam.cpp
    devicelimesdrshared.cpp
)

set(limesdrdevice_HEADERS
    devicelimesdr.h
    devicelimesdrparam.h
    devicelimesdrshared.h
)

include_directories(
    ${LIMESUITE_INCLUDE_DIR}
)

add_library(limesdrdevice SHARED
    ${limesdrdevice_SOURCES}
)

set_target_properties(limesdrdevice
    PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

target_link_libraries(limesdrdevice
    ${LIMESUITE_LIBRARY}
    sdrbase
)

if(LIMESUITE_EXTERNAL)
    add_dependencies(limesdrdevice limesuite)
endif()

install(TARGETS limesdrdevice DESTINATION ${INSTALL_LIB_DIR})
