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

    # need ffmpeg 3.1 that correstonds to
    # libavutil 55.27.100
    # libavcodec 57.48.101
    # libavformat 57.40.101
    # TODO use libavcodec/version.h
    find_package(FFmpeg)
    if (FFMPEG_FOUND)
      if(WIN32)
        add_subdirectory(demoddatv)
      else()
        # You can only get FFmpeg version from the command line
        EXECUTE_PROCESS(COMMAND ffmpeg -version COMMAND grep ffmpeg COMMAND cut -d\  -f3 COMMAND tr -d '\n' OUTPUT_VARIABLE FFMPEG_VERSION)
        message(STATUS "FFmpeg version ${FFMPEG_VERSION} found")
        if(FFMPEG_VERSION VERSION_GREATER "3.1")
            message(STATUS "Include demoddatv")
            add_subdirectory(demoddatv)
        else()
            message(STATUS "FFmpeg too old to compile demoddatv; needs at least ffmpeg 3.1")
        endif()
      endif()
    endif()
endif()
