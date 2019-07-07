project(mod)

add_subdirectory(modam)
add_subdirectory(modnfm)
add_subdirectory(modssb)
add_subdirectory(modwfm)
add_subdirectory(udpsource)
add_subdirectory(localsource)
add_subdirectory(filesource)

if(CM256CC_FOUND)
    add_subdirectory(remotesource)
endif(CM256CC_FOUND)

if (OpenCV_FOUND)
    add_subdirectory(modatv)
endif()

if (CODEC2_FOUND)
    add_subdirectory(modfreedv)
endif(CODEC2_FOUND)
