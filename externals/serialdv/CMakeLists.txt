project(serialdv)

set(serialdv_SOURCES
    ${LIBSERIALDVSRC}/dvcontroller.cpp  
    ${LIBSERIALDVSRC}/serialdatacontroller.cpp  
)

set(serialdv_HEADERS
    ${LIBSERIALDVSRC}/dvcontroller.h    
    ${LIBSERIALDVSRC}/serialdatacontroller.h    
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBSERIALDVSRC}
)

add_definitions(-DQT_SHARED)

add_library(serialdv SHARED
    ${serialdv_SOURCES}
)

set_target_properties(serialdv PROPERTIES DEFINE_SYMBOL "serialdv_EXPORTS")

target_link_libraries(serialdv
    ${LIBUSB_LIBRARIES}
)

install(TARGETS serialdv DESTINATION ${INSTALL_LIB_DIR})
