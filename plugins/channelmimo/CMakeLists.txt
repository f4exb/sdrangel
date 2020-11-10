project(channelmimo)

add_subdirectory(beamsteeringcwmod)

if (NOT SERVER_MODE)
    add_subdirectory(interferometer)
endif()
