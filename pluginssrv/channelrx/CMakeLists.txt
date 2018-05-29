project(demod)

add_subdirectory(demodam)
add_subdirectory(demodbfm)

if((LIBDSDCC_FOUND AND LIBMBE_FOUND) OR BUILD_DEBIAN)
    add_subdirectory(demoddsd)
endif()

add_subdirectory(demodnfm)
add_subdirectory(demodssb)
add_subdirectory(demodwfm)
add_subdirectory(udpsrc)
