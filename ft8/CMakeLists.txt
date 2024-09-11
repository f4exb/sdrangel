project(ft8)

set(ft8_SOURCES
    arrays.cpp
    fft.cpp
    fftbuffers.cpp
    ft8.cpp
    ft8plan.cpp
    ft8plans.cpp
    ft8stats.cpp
    libldpc.cpp
    osd.cpp
    packing.cpp
    pack0.cpp
    unpack0.cpp
    util.cpp
)

set(ft8_HEADERS
    arrays.h
    fft.h
    fftbuffers.h
    ft8.h
    ft8plan.h
    ft8plans.h
    ft8stats.h
    libldpc.h
    osd.h
    packing.h
    pack0.h
    unpack0.h
    util.h
)

include_directories(
    ${FFTW3F_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
)

add_library(ft8
    ${ft8_SOURCES}
)

if(DEFINED FFTW3F_DEPENDS)
    add_dependencies(ft8 "${FFTW3F_DEPENDS}")
endif()

target_link_libraries(ft8
    Boost::disable_autolinking
    ${FFTW3F_LIBRARIES}
    Qt::Core
    sdrbase
)

# remove or comment when debugging is done
# set_property(TARGET ft8 PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Og>")

install(TARGETS ft8 DESTINATION ${INSTALL_LIB_DIR})
