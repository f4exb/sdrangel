project(channelmimo)

add_subdirectory(beamsteeringcwsource)

if (NOT SERVER_MODE)
    add_subdirectory(interferometer)
endif()
