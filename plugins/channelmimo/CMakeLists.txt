project(channelmimo)

add_subdirectory(beamsteeringcwmod)
add_subdirectory(doa2)

if (NOT SERVER_MODE)
    add_subdirectory(interferometer)
endif()
