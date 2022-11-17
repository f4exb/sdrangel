project (sdrbench)

set(sdrbench_SOURCES
    mainbench.cpp
    parserbench.cpp
    test_golay2312.cpp
)

set(sdrbench_HEADERS
    mainbench.h
    parserbench.h
)

add_library(sdrbench SHARED
    ${sdrbench_SOURCES}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/logging
)

target_link_libraries(sdrbench
    Qt::Core
    Qt::Gui
    sdrbase
    logging
)

install(TARGETS sdrbench DESTINATION ${INSTALL_LIB_DIR})
