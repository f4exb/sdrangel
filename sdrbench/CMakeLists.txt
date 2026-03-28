project (sdrbench)

if (FT8_SUPPORT)
    set(sdrbench_FT8_LIB ft8)
endif()

set(sdrbench_SOURCES
    mainbench.cpp
    parserbench.cpp
    test_golay2312.cpp
    test_ft8.cpp
    test_ft4.cpp
    test_callsign.cpp
    test_ft8protocols.cpp
    test_fftrrc.cpp
    test_firrrc.cpp
    test_meshtastic.cpp
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
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/logging
    ${CMAKE_SOURCE_DIR}/modemmeshtastic
    ${CMAKE_SOURCE_DIR}
)

if(DEFINED FFTW3F_DEPENDS)
    add_dependencies(sdrbench "${FFTW3F_DEPENDS}")
endif()

target_link_libraries(sdrbench
     Boost::disable_autolinking
    ${FFTW3F_LIBRARIES}
    Qt::Core
    Qt::Gui
    sdrbase
    logging
    modemmeshtastic
    ${sdrbench_FT8_LIB}
)

install(TARGETS sdrbench DESTINATION ${INSTALL_LIB_DIR})
