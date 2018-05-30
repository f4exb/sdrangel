project (sdrbench)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopt-info-vec-optimized")

set(sdrbench_SOURCES
    mainbench.cpp
    parserbench.cpp
)

set(sdrbench_HEADERS
    mainbench.h
    parserbench.h
)

set(sdrbench_SOURCES
    ${sdrbench_SOURCES}
    ${sdrbench_HEADERS}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(sdrbench SHARED
    ${sdrbench_SOURCES}
    ${sdrbench_HEADERS_MOC}
)

include_directories(
    .
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase    
    ${CMAKE_SOURCE_DIR}/logging
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(sdrbench
    ${QT_LIBRARIES}
    sdrbase
    logging
)

target_compile_features(sdrbench PRIVATE cxx_generalized_initializers) # cmake >= 3.1.0

target_link_libraries(sdrbench Qt5::Core Qt5::Gui)

install(TARGETS sdrbench DESTINATION lib)

