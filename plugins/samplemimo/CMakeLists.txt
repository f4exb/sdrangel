project(samplemimo)

if(ENABLE_BLADERF AND LIBBLADERF_FOUND)
    add_subdirectory(bladerf2mimo)
endif()

if(ENABLE_LIMESUITE AND LIMESUITE_FOUND)
    add_subdirectory(limesdrmimo)
endif()

add_subdirectory(testmi)
add_subdirectory(testmosync)
