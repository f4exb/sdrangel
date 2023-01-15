project(demod)

if (ENABLE_CHANNELRX_DEMODADSB AND Qt${QT_DEFAULT_MAJOR_VERSION}Quick_FOUND AND Qt${QT_DEFAULT_MAJOR_VERSION}QuickWidgets_FOUND AND Qt${QT_DEFAULT_MAJOR_VERSION}Positioning_FOUND AND Qt${QT_DEFAULT_MAJOR_VERSION}TextToSpeech_FOUND)
    add_subdirectory(demodadsb)
    # add_subdirectory(demodvormc)
endif()

if (ENABLE_CHANNELRX_DEMODAM)
    add_subdirectory(demodam)
endif()

if (ENABLE_CHANNELRX_DEMODBFM)
    add_subdirectory(demodbfm)
endif()

if (ENABLE_CHANNELRX_DEMODNFM)
    add_subdirectory(demodnfm)
endif()

if (ENABLE_CHANNELRX_DEMODSSB)
    add_subdirectory(demodssb)
endif()

if (ENABLE_CHANNELRX_UDPSINK)
    add_subdirectory(udpsink)
endif()

if (ENABLE_CHANNELRX_DEMODWFM)
    add_subdirectory(demodwfm)
endif()

if (ENABLE_CHANNELRX_LOCALSINK)
    add_subdirectory(localsink)
endif()

if (ENABLE_CHANNELRX_FILESINK)
    add_subdirectory(filesink)
endif()

if (ENABLE_CHANNELRX_FREQTRACKER)
    add_subdirectory(freqtracker)
endif()

if (ENABLE_CHANNELRX_DEMODCHIRPCHAT)
    add_subdirectory(demodchirpchat)
endif()

if (ENABLE_CHANNELRX_DEMODVOR)
    add_subdirectory(demodvor)
endif()

if (ENABLE_CHANNELRX_DEMODPACKET)
    add_subdirectory(demodpacket)
endif()

if (ENABLE_CHANNELRX_DEMODAIS)
    add_subdirectory(demodais)
endif()

if (ENABLE_CHANNELRX_DEMODPAGER)
    add_subdirectory(demodpager)
endif()

if (ENABLE_CHANNELRX_RADIOCLOCK)
    add_subdirectory(radioclock)
endif()

if (ENABLE_CHANNELRX_RADIOASTRONOMY)
    add_subdirectory(radioastronomy)
endif()

if (ENABLE_CHANNELRX_DEMODRADIOSONDE)
    add_subdirectory(demodradiosonde)
endif()

if(ENABLE_CHANNELRX_DEMODDAB AND DAB_FOUND AND ZLIB_FOUND AND FAAD_FOUND)
    add_subdirectory(demoddab)
endif()

if(ENABLE_CHANNELRX_DEMODAPT AND APT_FOUND AND SGP4_FOUND)
    add_subdirectory(demodapt)
endif()

if(ENABLE_CHANNELRX_DEMODDSD AND LIBDSDCC_FOUND AND LIBMBE_FOUND)
    add_subdirectory(demoddsd)
endif()

if (ENABLE_CHANNELRX_REMOTESINK AND CM256CC_FOUND AND (HAS_SSE3 OR HAS_NEON))
    add_subdirectory(remotesink)
endif()

if (ENABLE_CHANNELRX_REMOTETCPSINK)
    add_subdirectory(remotetcpsink)
endif()

if (ENABLE_CHANNELRX_DEMODFREEDV AND CODEC2_FOUND)
    add_subdirectory(demodfreedv)
endif()

if (ENABLE_CHANNELRX_DEMODM17 AND CODEC2_FOUND)
    add_subdirectory(demodm17)
endif()

if (ENABLE_CHANNELRX_SIGMFFILESINK AND LIBSIGMF_FOUND AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_subdirectory(sigmffilesink)
endif()

if (ENABLE_CHANNELRX_NOISEFIGURE AND Boost_FOUND AND Boost_VERSION_STRING VERSION_GREATER_EQUAL "1.70.0")
    add_subdirectory(noisefigure)
endif()

if (ENABLE_CHANNELRX_DEMODFT8 AND FT8_SUPPORT)
    add_subdirectory(demodft8)
endif()

if(NOT SERVER_MODE)
    if (ENABLE_CHANNELRX_CHANALYZER)
        add_subdirectory(chanalyzer)
    endif()

    if (ENABLE_CHANNELRX_DEMODATV)
        add_subdirectory(demodatv)
    endif()

    # need ffmpeg 3.1 that correstonds to
    # libavutil 55.27.100
    # libavcodec 57.48.101
    # libavformat 57.40.101
    find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)

    if (ENABLE_CHANNELRX_DEMODDATV AND FFMPEG_FOUND)
      if(WIN32)
        add_subdirectory(demoddatv)
      else()
        if(((AVUTIL_VERSION VERSION_GREATER "55.27.99") AND (AVCODEC_VERSION VERSION_GREATER "57.48.101")) OR FFMPEG_EXTERNAL OR FFMEG_SKIP_CHECK)
            message(STATUS "Include demoddatv")
            add_subdirectory(demoddatv)
        else()
            message(STATUS "FFmpeg too old to compile demoddatv; needs at least avutil: 55.27.100, avcodec/avformat: 57.48.101")
        endif()
      endif()
    endif()
endif()
