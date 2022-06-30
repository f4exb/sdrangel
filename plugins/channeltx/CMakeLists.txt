project(mod)

if (ENABLE_CHANNELTX_MODAIS)
    add_subdirectory(modais)
endif()

if (ENABLE_CHANNELTX_MODAM)
    add_subdirectory(modam)
endif()

if (ENABLE_CHANNELTX_MODCHIRPCHAT)
    add_subdirectory(modchirpchat)
endif()

if (ENABLE_CHANNELTX_MODNFM)
    add_subdirectory(modnfm)
endif()

if (ENABLE_CHANNELTX_MODSSB)
    add_subdirectory(modssb)
endif()

if (ENABLE_CHANNELTX_MODWFM)
    add_subdirectory(modwfm)
endif()

if (ENABLE_CHANNELTX_MODPACKET)
    add_subdirectory(modpacket)
endif()

if (ENABLE_CHANNELTX_MOD802.15.4)
    add_subdirectory(mod802.15.4)
endif()

if (ENABLE_CHANNELTX_UDPSOURCE)
    add_subdirectory(udpsource)
endif()

if (ENABLE_CHANNELTX_LOCALSOURCE)
    add_subdirectory(localsource)
endif()

if (ENABLE_CHANNELTX_FILESOURCE)
    add_subdirectory(filesource)
endif()

if (ENABLE_CHANNELTX_REMOTESOURCE AND CM256CC_FOUND AND (HAS_SSE3 OR HAS_NEON))
    add_subdirectory(remotesource)
endif()

if (ENABLE_CHANNELTX_MODATV AND OpenCV_FOUND)
    add_subdirectory(modatv)
endif()

# Copied from channelrx/CMakeLists.txt - why not in top-level?
find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)

if (ENABLE_CHANNELTX_MODDATV AND FFMPEG_FOUND)
    add_subdirectory(moddatv)
endif()

if (ENABLE_CHANNELTX_MODFREEDV AND CODEC2_FOUND)
    add_subdirectory(modfreedv)
endif()

if (ENABLE_CHANNELTX_MODM17 AND CODEC2_FOUND)
    add_subdirectory(modm17)
endif()
