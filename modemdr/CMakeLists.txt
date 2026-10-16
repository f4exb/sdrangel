project(modemdr)

# Data services and coding shared by the DAB and DRM receivers (modemdab and modemdrm): MOT,
# Journaline, SPI, text, CRCs and inflate. It has no Qt dependency.

set(modemdr_SOURCES
    crc.cpp
    inflate.cpp
    journaline.cpp
    mot.cpp
    spi.cpp
    text.cpp
    viterbi.cpp
)

set(modemdr_HEADERS
    crc.h
    inflate.h
    journaline.h
    mot.h
    spi.h
    text.h
    viterbi.h
)

add_library(modemdr ${modemdr_SOURCES})

target_include_directories(modemdr
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/exports
)

install(TARGETS modemdr DESTINATION ${INSTALL_LIB_DIR})
