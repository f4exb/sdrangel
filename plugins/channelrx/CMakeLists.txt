project(demod)

if (Qt5Quick_FOUND AND Qt5QuickWidgets_FOUND AND Qt5Positioning_FOUND)
    add_subdirectory(demodadsb)
endif()
add_subdirectory(demodam)
add_subdirectory(demodbfm)
add_subdirectory(demodnfm)
add_subdirectory(demodssb)
add_subdirectory(udpsink)
add_subdirectory(demodwfm)
add_subdirectory(localsink)
add_subdirectory(filesink)
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
    find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)
    if (FFMPEG_FOUND)
      if(WIN32)
        add_subdirectory(demoddatv)
      else()
        if((AVUTIL_VERSION VERSION_GREATER "55.27.99") AND (AVCODEC_VERSION VERSION_GREATER "57.48.101"))
            message(STATUS "Include demoddatv")
            add_subdirectory(demoddatv)
        else()
            message(STATUS "FFmpeg too old to compile demoddatv; needs at least avutil: 55.27.100, avcodec/avformat: 57.48.101")
        endif()
      endif()
    endif()
endif()
