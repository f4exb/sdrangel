project(ft8)

set(ft8_SOURCES
    fft.cpp
    ft8.cpp
    libldpc.cpp
    osd.cpp
    unpack.cpp
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

add_library(ft8 SHARED
    ${ft8_SOURCES}
)

target_link_libraries(ft8
    ${FFTW3F_LIBRARIES}
)

install(TARGETS ft8 DESTINATION ${INSTALL_LIB_DIR})
