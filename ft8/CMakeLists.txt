project(ft8)

set(ft8_SOURCES
    fft.cpp
    ft8.cpp
    libldpc.cpp
    osd.cpp
    unpack.cpp
    unpack0.cpp
    util.cpp
)

set(ft8_HEADERS
    fft.h
    ft8.h
    libldpc.h
    osd.h
    unpack.h
    util.h
)

include_directories(
    ${FFTW3F_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
)

add_library(ft8 SHARED
    ${ft8_SOURCES}
)

target_link_libraries(ft8
    Boost::disable_autolinking
    ${FFTW3F_LIBRARIES}
    Qt::Core
    sdrbase
)

# remove or comment when debugging is done
# set_property(TARGET ft8 PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Og>")

install(TARGETS ft8 DESTINATION ${INSTALL_LIB_DIR})
