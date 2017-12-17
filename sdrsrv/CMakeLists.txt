project (sdrsrv)

set(sdrsrv_SOURCES
    maincore.cpp   
    device/deviceset.cpp 
    webapi/webapiadaptersrv.cpp
)

set(sdrsrv_HEADERS
    maincore.h
    device/deviceset.h
    webapi/webapiadaptersrv.h
)

set(sdrsrv_SOURCES
    ${sdrsrv_SOURCES}
    ${sdrsrv_HEADERS}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(sdrsrv SHARED
    ${sdrsrv_SOURCES}
    ${sdrsrv_HEADERS_MOC}
)

include_directories(
    .
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/logging
    ${CMAKE_SOURCE_DIR}/httpserver
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_CURRENT_BINARY_DIR}
    ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(sdrsrv
    ${QT_LIBRARIES}
    sdrbase
    logging
)

set_target_properties(sdrsrv PROPERTIES DEFINE_SYMBOL "sdrangel_EXPORTS")
target_compile_features(sdrsrv PRIVATE cxx_generalized_initializers) # cmake >= 3.1.0

qt5_use_modules(sdrsrv Core Multimedia)

install(TARGETS sdrsrv DESTINATION lib)

