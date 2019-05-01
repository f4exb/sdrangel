project(demod)


add_subdirectory(demodam)
add_subdirectory(demodbfm)
add_subdirectory(demodnfm)
add_subdirectory(demodssb)
add_subdirectory(udpsink)
add_subdirectory(demodwfm)
add_subdirectory(localsink)
add_subdirectory(freqtracker)

if(LIBDSDCC_FOUND AND LIBMBE_FOUND)
    add_subdirectory(demoddsd)
endif(LIBDSDCC_FOUND AND LIBMBE_FOUND)

if(CM256CC_FOUND)
    add_subdirectory(remotesink)
endif(CM256CC_FOUND)

if (CODEC2_FOUND)
    add_subdirectory(demodfreedv)
endif(CODEC2_FOUND)

if(NOT SERVER_MODE)
    add_subdirectory(demodlora)
    add_subdirectory(chanalyzer)
    add_subdirectory(demodatv)

    # At ffmpeg 3.1 !! TODO check more !!
    # libavutil 55.27.100
    # libavcodec 57.48.101
    # libavformat 57.40.101
    find_package(FFmpeg 57.48.101)
    if (FFMPEG_FOUND)
        message(STATUS "Include demoddatv")
        add_subdirectory(demoddatv)
    endif()
endif()
