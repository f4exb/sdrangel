project (sdrbench)

set(sdrbench_SOURCES
    mainbench.cpp
    parserbench.cpp
    test_golay2312.cpp
    test_ft8.cpp
)

set(sdrbench_HEADERS
    mainbench.h
    parserbench.h
)

add_library(sdrbench SHARED
    ${sdrbench_SOURCES}
)

include_directories(
    ${FFTW3F_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/logging
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sdrbench
    ${FFTW3F_LIBRARIES}
    Qt::Core
    Qt::Gui
    sdrbase
    logging
    ft8
)

install(TARGETS sdrbench DESTINATION ${INSTALL_LIB_DIR})
