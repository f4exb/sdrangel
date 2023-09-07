project(mod)

if (ENABLE_CHANNELTX_MODPSK31)
    add_subdirectory(modpsk31)
endif()

if (ENABLE_CHANNELTX_MODRTTY)
    add_subdirectory(modrtty)
endif()

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
else()
    message(STATUS "Not building remotesource (ENABLE_CHANNELTX_REMOTESOURCE=${ENABLE_CHANNELTX_REMOTESOURCE} CM256CC_FOUND=${CM256CC_FOUND} HAS_SSE3=${HAS_SSE3} HAS_NEON=${HAS_NEON})")
endif()

if (ENABLE_CHANNELTX_MODATV AND OpenCV_FOUND)
    add_subdirectory(modatv)
else()
    message(STATUS "Not building modatv (ENABLE_CHANNELTX_MODATV=${ENABLE_CHANNELTX_MODATV} OpenCV_FOUND=${OpenCV_FOUND})")
endif()

# Copied from channelrx/CMakeLists.txt - why not in top-level?
find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)

if (ENABLE_CHANNELTX_MODDATV AND FFMPEG_FOUND)
    add_subdirectory(moddatv)
else()
    message(STATUS "Not building moddatv (ENABLE_CHANNELTX_MODDATV=${ENABLE_CHANNELTX_MODDATV} FFMPEG_FOUND=${FFMPEG_FOUND})")
endif()

if (ENABLE_CHANNELTX_MODFREEDV AND CODEC2_FOUND)
    add_subdirectory(modfreedv)
else()
    message(STATUS "Not building modfreedv (ENABLE_CHANNELTX_MODFREEDV=${ENABLE_CHANNELTX_MODFREEDV} CODEC2_FOUND=${CODEC2_FOUND})")
endif()

if (ENABLE_CHANNELTX_MODM17 AND CODEC2_FOUND)
    add_subdirectory(modm17)
else()
    message(STATUS "Not building modm17 (ENABLE_CHANNELTX_MODM17=${ENABLE_CHANNELTX_MODM17} CODEC2_FOUND=${CODEC2_FOUND})")
endif()
