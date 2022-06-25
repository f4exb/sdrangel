project(channelmimo)

if (ENABLE_CHANNELMIMO_BEAMSTEERINGCWMOD)
    add_subdirectory(beamsteeringcwmod)
endif()

if (ENABLE_CHANNELMIMO_DOA2)
    add_subdirectory(doa2)
endif()

if (NOT SERVER_MODE AND ENABLE_CHANNELMIMO_INTERFEROMETER)
    add_subdirectory(interferometer)
endif()
