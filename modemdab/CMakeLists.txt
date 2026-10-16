project(modemdab)

set(modemdab_SOURCES
    dabaudiodecoder.cpp
    dabaudioframing.cpp
    dabcoherent.cpp
    dabconvolutional.cpp
    dabensemble.cpp
    dabfft.cpp
    dabmsc.cpp
    dabofdm.cpp
    dabpad.cpp
    dabprofile.cpp
    dabreceiver.cpp
    dabtables.cpp
    dabtii.cpp
)

set(modemdab_HEADERS
    dabaudiodecoder.h
    dabaudioframing.h
    dabcoherent.h
    dabconvolutional.h
    dabensemble.h
    dabfft.h
    dabmsc.h
    dabofdm.h
    dabpad.h
    dabprofile.h
    dabreceiver.h
    dabtables.h
    dabtii.h
)

add_library(modemdab ${modemdab_SOURCES})

target_include_directories(modemdab
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/exports
    PRIVATE
        ${CMAKE_SOURCE_DIR}/sdrbase
)

# MOT, Journaline and SPI are shared with DRM
target_link_libraries(modemdab PUBLIC modemdr)

# FFTW for the FFTs, when there is one (kissfft otherwise)
if(FFTW3F_FOUND)
    target_compile_definitions(modemdab PRIVATE MODEMDAB_HAS_FFTW=1)
    # The planner is shared with the rest of SDRangel: make it thread safe when FFTW can
    include(CheckFunctionExists)
    set(CMAKE_REQUIRED_LIBRARIES ${FFTW3F_LIBRARIES})
    check_function_exists(fftwf_make_planner_thread_safe MODEMDAB_FFTW_THREAD_SAFE)
    unset(CMAKE_REQUIRED_LIBRARIES)
    if(MODEMDAB_FFTW_THREAD_SAFE)
        target_compile_definitions(modemdab PRIVATE MODEMDAB_FFTW_THREAD_SAFE=1)
    endif()
    target_include_directories(modemdab PRIVATE ${FFTW3F_INCLUDE_DIRS})
    target_link_libraries(modemdab PRIVATE ${FFTW3F_LIBRARIES})
    if(DEFINED FFTW3F_DEPENDS)
        add_dependencies(modemdab ${FFTW3F_DEPENDS})
    endif()
endif()

# DAB+ (HE-AAC v2) audio is decoded by FAAD2
if(FAAD_FOUND)
    message(STATUS "modemdab: DAB+ audio via FAAD2")
    target_compile_definitions(modemdab PRIVATE MODEMDAB_HAS_FAAD2=1)
    target_include_directories(modemdab PRIVATE ${FAAD_INCLUDE_DIR})
    target_link_libraries(modemdab PRIVATE ${FAAD_LIBRARY})
    if(DEFINED FAAD_DEPENDS)
        add_dependencies(modemdab ${FAAD_DEPENDS})
    endif()
else()
    message(STATUS "modemdab: no DAB+ audio codec (FAAD2 not found)")
endif()

# DAB (MPEG Layer II) audio is decoded by FFmpeg's libavcodec
if(NOT FFMPEG_FOUND)
    find_package(FFmpeg COMPONENTS AVCODEC AVUTIL)
endif()
if(FFMPEG_FOUND)
    message(STATUS "modemdab: DAB audio via FFmpeg")
    target_compile_definitions(modemdab PRIVATE MODEMDAB_HAS_FFMPEG=1)
    target_include_directories(modemdab PRIVATE ${AVCODEC_INCLUDE_DIRS} ${AVUTIL_INCLUDE_DIRS})
    target_link_libraries(modemdab PRIVATE ${AVCODEC_LIBRARIES} ${AVUTIL_LIBRARIES})
    if(DEFINED FFMPEG_DEPENDS)
        add_dependencies(modemdab ${FFMPEG_DEPENDS})
    endif()
else()
    message(STATUS "modemdab: no DAB (MPEG Layer II) audio codec (FFmpeg not found)")
endif()

install(TARGETS modemdab DESTINATION ${INSTALL_LIB_DIR})
