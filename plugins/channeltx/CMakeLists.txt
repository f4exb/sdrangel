project(mod)

add_subdirectory(modais)
add_subdirectory(modam)
add_subdirectory(modchirpchat)
add_subdirectory(modnfm)
add_subdirectory(modssb)
add_subdirectory(modwfm)
add_subdirectory(modpacket)
add_subdirectory(mod802.15.4)
add_subdirectory(udpsource)
add_subdirectory(localsource)
add_subdirectory(filesource)

if (CM256CC_FOUND AND (HAS_SSE3 OR HAS_NEON))
    add_subdirectory(remotesource)
endif()

if (OpenCV_FOUND)
    add_subdirectory(modatv)
endif()

# Copied from channelrx/CMakeLists.txt - why not in top-level?
find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)
if (FFMPEG_FOUND)
    add_subdirectory(moddatv)
endif()

if (CODEC2_FOUND)
    add_subdirectory(modfreedv)
endif(CODEC2_FOUND)
