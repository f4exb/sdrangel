project(samplemimo)

if(ENABLE_BLADERF AND LIBBLADERF_FOUND)
    add_subdirectory(bladerf2mimo)
endif()

add_subdirectory(testmi)
