project(demod)

add_subdirectory(demodam)
add_subdirectory(demodbfm)

if((LIBDSDCC_FOUND AND LIBMBE_FOUND) OR BUILD_DEBIAN)
    add_subdirectory(demoddsd)
endif()

find_package(CM256cc)
if(CM256CC_FOUND)
    add_subdirectory(remotesink)
endif(CM256CC_FOUND)

find_package(Codec2)
if (CODEC2_FOUND)
    add_subdirectory(demodfreedv)
endif(CODEC2_FOUND)

add_subdirectory(demodnfm)
add_subdirectory(demodssb)
add_subdirectory(demodwfm)
add_subdirectory(udpsink)
